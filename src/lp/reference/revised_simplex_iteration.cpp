#include "revised_simplex_internal.hpp"
namespace markov_cero::lp::reference {
using namespace detail_revised_simplex;
namespace detail_revised_simplex {
IterationOutcome iterate(Work& w, const std::vector<double>& cost, std::size_t enter_limit,
                         const Options& o, int phase, std::size_t budget,
                         std::vector<IterationRecord>& log, bool& telemetry_truncated) {
    IterationOutcome out;
    std::vector<char> rejected_column(w.total_columns, 0);
    std::vector<unsigned char> reject_revivals(w.total_columns, 0);
    bool artificial_reset_used = false;
    const auto s_opts = sparse_options(o);
    auto factor = make_factor(w, s_opts);
    std::size_t degenerate_steps = 0;
    double last_obj = std::numeric_limits<double>::quiet_NaN();
    const double b_scale = inf_norm(w.b);
    for (std::size_t step = 0; step <= budget; ++step) {
        if (o.deadline && std::chrono::steady_clock::now() >= *o.deadline) {
            out.status = SolveStatus::resource_limit;
            out.iterations = step;
            record_condition(factor, out);
            return out;
        }
        if (step == budget) {
            out.status = SolveStatus::iteration_limit;
            out.iterations = step;
            record_condition(factor, out);
            return out;
        }
        bool restart = false;
        // Unified numerical recovery: in phase I a corrupt basis restarts once
        // from the always-factorable artificial basis (identity columns, so the
        // restart itself cannot reproduce the corruption); any later failure —
        // or any failure in phase II, which has no feasible fallback basis —
        // is reported honestly as NumericalFailure with the site's reason.
        auto recover = [&](const std::string& why) -> bool {
            return recover_step(w, factor, s_opts, phase, artificial_reset_used, out, step,
                                restart, why);
        };
        while (true) {
            auto xb = factor.solve(w.b);
            const auto xb_gate_ok = [&] {
                try {
                    return linalg::sparse_infinity_residual(factor.current_basis(), xb, w.b) <=
                           drift_gate(b_scale);
                } catch (const std::exception&) {
                    return false;
                }
            };
            if (!xb_gate_ok()) {
                // Chain stale or basis corrupt: rebuild the LU of the same
                // basis and re-solve before touching the pivot sequence.
                try {
                    factor.refactorize();
                    xb = factor.solve(w.b);
                } catch (const std::exception&) {
                    // fall through to the gate check below
                }
                if (!xb_gate_ok()) {
                    if (!recover("primal basis solve residual exceeded drift gate"))
                        return out;
                    break;
                }
            }
            bool has_neg = false;
            for (double v : xb) {
                if (v < -1e-5) {
                    has_neg = true;
                    break;
                }
            }
            if (has_neg) {
                // The eta chain still factorizes but its solution no longer
                // respects primal feasibility: rebuild from scratch. If the
                // basis itself no longer factorizes, recover.
                try {
                    factor = make_factor(w, s_opts);
                    xb = factor.solve(w.b);
                } catch (const std::exception& e) {
                    if (!recover(std::string("basis factorization failed: ") + e.what()))
                        return out;
                    break;
                }
            }
            try {
                snap_basic_solution(xb, o.feasibility_tolerance);
            } catch (const std::exception& e) {
                if (!recover(std::string(e.what()) + " (basic solution below feasibility)")
                        )
                    return out;
                break;
            }
            for (double value : xb) {
                if (!std::isfinite(value) || std::abs(value) > 1e30) {
                    if (!recover("non-finite or divergent basic solution")) return out;
                    break;
                }
            }
            if (restart) break;
            std::vector<double> cb(w.rows);
            std::vector<bool> basic(w.total_columns);
            for (std::size_t i = 0; i < w.rows; ++i) {
                cb[i] = cost[w.basis[i]];
                basic[w.basis[i]] = true;
            }
            const double cb_scale = inf_norm(cb);
            const auto solve_y = [&]() {
                const auto residual_of = [&](const std::vector<double>& cand,
                                             std::vector<double>& out_res) {
                    double worst = 0;
                    for (std::size_t j = 0; j < w.rows; ++j) {
                        long double value = cb[j];
                        for (const auto& [i, coefficient] : w.a[w.basis[j]])
                            value -= static_cast<long double>(coefficient) * cand[i];
                        out_res[j] = static_cast<double>(value);
                        worst = std::max(worst, std::abs(out_res[j]));
                    }
                    return worst;
                };
                auto yy = factor.solve_transpose(cb);
                std::vector<double> residual(w.rows);
                double worst = residual_of(yy, residual);
                for (int refinement = 0; refinement < 2 && worst > 0.0; ++refinement) {
                    const auto correction = factor.solve_transpose(residual);
                    std::vector<double> candidate = yy;
                    for (std::size_t i = 0; i < w.rows; ++i) candidate[i] += correction[i];
                    std::vector<double> candidate_residual(w.rows);
                    const double candidate_worst = residual_of(candidate, candidate_residual);
                    if (candidate_worst >= worst) {
                        // Noise floor: on ill-conditioned bases (bore3d carried
                        // |y| ~ 1e11) a further correction only amplifies the
                        // roundoff it is meant to remove; keep the better y.
                        break;
                    }
                    yy = std::move(candidate);
                    residual = std::move(candidate_residual);
                    worst = candidate_worst;
                }
                return yy;
            };
            const auto y_gate_ok = [&](const std::vector<double>& yy) {
                try {
                    return linalg::sparse_infinity_residual(factor.current_basis(), yy, cb,
                                                            /*transpose=*/true) <=
                           drift_gate(cb_scale);
                } catch (const std::exception&) {
                    return false;
                }
            };
            auto y = solve_y();
            if (!y_gate_ok(y)) {
                // A fresh factorization of the same basis is all an in-place
                // heal can offer; if that does not restore the dual residual,
                // route through recovery (restart in phase I, honest failure
                // in phase II).
                try {
                    factor.refactorize();
                    xb = factor.solve(w.b);
                    y = solve_y();
                } catch (const std::exception&) {
                    // fall through to the gate check below
                }
                if (!y_gate_ok(y)) {
                    if (!recover("dual basis solve residual exceeded drift gate")) return out;
                    break;
                }
            }

            const double current_obj = dot(cb, xb);
            if (std::isnan(last_obj) || std::abs(current_obj - last_obj) > 1e-9) {
                degenerate_steps = 0;
            } else {
                ++degenerate_steps;
            }
            last_obj = current_obj;

            // Bland pricing guarantees finite termination, but at a crawl on
            // long frozen faces: under the old 20-step trigger netlib scsd1's
            // phase II switched to Bland after its first degenerate stretch
            // and wandered 230085 pivots where pure Dantzig finishes in 1023
            // (longest consecutive frozen run 783; every other netlib model
            // stays under 124). 5000 sits far above the measured runs yet
            // below the default iteration budget, so a genuine cycle still
            // reaches Bland repair inside a single solve.
            constexpr std::size_t kAntiCyclingDegenerateSteps = 5000;
            const bool use_bland =
                o.bland_anti_cycling || degenerate_steps >= kAntiCyclingDegenerateSteps;

            std::size_t entering = enter_limit;
            std::size_t ejected = w.rows;
            double theta = 0;
            double minimum_rc = 0;

            std::vector<bool> candidate_tried(enter_limit, false);
            for (std::size_t j = 0; j < enter_limit; ++j) {
                candidate_tried[j] = rejected_column[j] != 0;
            }
            const auto action = price_and_trial(w, cost, y, xb, basic, o, phase, step,
                                                use_bland, enter_limit, factor, s_opts,
                                                b_scale, rejected_column, reject_revivals,
                                                candidate_tried, out, artificial_reset_used,
                                                restart);
            if (action.kind == PivotAction::finished) return out;
            entering = action.entering;
            ejected = action.ejected;
            theta = action.theta;
            minimum_rc = action.minimum_rc;
            if (restart) break;
            if (log.size() < o.telemetry_limit) {
                log.push_back({log.size(), phase, dot(cb, xb), minimum_rc, entering, ejected,
                               theta <= o.feasibility_tolerance});
            } else {
                telemetry_truncated = true;
            }
            break;
        }
        if (restart) {
            // Recovery restarted the step from the artificial basis; the
            // restart itself is not a pivot and must not consume budget.
            --step;
            continue;
        }
    }
    record_condition(factor, out);
    return out;
}
}

}
