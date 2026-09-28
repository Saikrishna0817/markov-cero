#include "revised_simplex_internal.hpp"
namespace markov_cero::lp::reference {
using namespace detail_revised_simplex;
namespace detail_revised_simplex {
IterationOutcome iterate(Work& w, const std::vector<double>& cost, std::size_t enter_limit,
                         const Options& o, int phase, std::size_t budget,
                         std::vector<IterationRecord>& log, bool& telemetry_truncated) {
    IterationOutcome out;
    std::vector<char> rejected_column(w.total_columns, 0);
    bool artificial_reset_used = false;
    const auto s_opts = sparse_options(o);
    auto factor = make_factor(w, s_opts);
    std::deque<std::pair<std::size_t, std::size_t>> recent_pivots;
    std::size_t degenerate_steps = 0;
    double last_obj = std::numeric_limits<double>::quiet_NaN();
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
        auto xb = factor.solve(w.b);
        bool has_neg = false;
        for (double v : xb) {
            if (v < -1e-5) {
                has_neg = true;
                break;
            }
        }
        if (has_neg) {

            try {
                factor = make_factor(w, s_opts);
                xb = factor.solve(w.b);
            } catch (const std::exception& e) {
                // The accumulated basis itself no longer factorizes: the update
                // chain drifted into numerical singularity. Phase I carries a
                // always-feasible identity fallback (original_columns + i), so
                // reset to it and re-enter phase I from the artificial basis;
                // in phase II report numerical failure honestly.
                if (phase != 1 || artificial_reset_used) {
                    // One reset per phase-I: a second singularity means the
                    // drift is structural, and re-entering from the artificial
                    // basis would only burn the iteration budget in a loop.
                    out.status = SolveStatus::numerical_failure;
                    out.iterations = step;
                    record_condition(factor, out);
                    return out;
                }
                artificial_reset_used = true;
                for (std::size_t i = 0; i < w.rows; ++i) {
                    w.basis[i] = w.original_columns + i;
                }
                factor = make_factor(w, s_opts);
                xb = factor.solve(w.b);
                (void)e;
            }
        }
        try {
            snap_basic_solution(xb, o.feasibility_tolerance);
        } catch (const std::exception& e) {

            throw;
        }
        for (double value : xb) {
            if (!std::isfinite(value) || std::abs(value) > 1e30) {
                out.status = SolveStatus::numerical_failure;
                out.iterations = step;
                record_condition(factor, out);
                return out;
            }
        }
        std::vector<double> cb(w.rows);
        std::vector<bool> basic(w.total_columns);
        for (std::size_t i = 0; i < w.rows; ++i) {
            cb[i] = cost[w.basis[i]];
            basic[w.basis[i]] = true;
        }
        auto y = factor.solve_transpose(cb);
        for (int refinement = 0; refinement < 2; ++refinement) {
            std::vector<double> residual(w.rows);
            for (std::size_t j = 0; j < w.rows; ++j) {
                long double value = cb[j];
                for (const auto& [i, coefficient] : w.a[w.basis[j]])
                    value -= static_cast<long double>(coefficient) * y[i];
                residual[j] = static_cast<double>(value);
            }
            const auto correction = factor.solve_transpose(residual);
            for (std::size_t i = 0; i < w.rows; ++i) y[i] += correction[i];
        }

        const double current_obj = dot(cb, xb);
        if (std::isnan(last_obj) || std::abs(current_obj - last_obj) > 1e-9) {
            degenerate_steps = 0;
            recent_pivots.clear();
        } else {
            ++degenerate_steps;
        }
        last_obj = current_obj;

        const bool use_bland = o.bland_anti_cycling || (degenerate_steps >= 20);

        std::size_t entering = enter_limit;
        std::size_t leaving_row = w.rows;
        std::vector<double> d;
        double theta = 0;
        double minimum_rc = 0;

        std::vector<bool> candidate_tried(enter_limit, false);
        for (std::size_t j = 0; j < enter_limit; ++j) {
            candidate_tried[j] = rejected_column[j] != 0;
        }
        while (true) {
            entering = enter_limit;
            minimum_rc = 0;
            for (std::size_t j = 0; j < enter_limit; ++j) {
                if (basic[j] || candidate_tried[j]) {
                    continue;
                }
                const double rc = cost[j] - column_dot(w, j, y);
                if (!significant_negative_reduced_cost(w, j, cost, y, rc, o.dual_tolerance)) {
                    continue;
                }
                if (use_bland) {
                    entering = j;
                    minimum_rc = rc;
                    break;
                }
                if (entering == enter_limit || rc < minimum_rc) {
                    entering = j;
                    minimum_rc = rc;
                }
            }
            if (entering == enter_limit) {
                out.status = SolveStatus::optimal;
                out.xb = std::move(xb);
                out.y = std::move(y);
                out.iterations = step;
                record_condition(factor, out);
                return out;
            }
            d = factor.solve(column(w, entering));
            leaving_row = select_leaving(w, xb, d, o, theta);
            if (leaving_row < w.rows) {
                const auto candidate_leaving = w.basis[leaving_row];
                bool is_cycling = false;
                for (const auto& p : recent_pivots) {
                    if ((p.first == candidate_leaving && p.second == entering) ||
                        (p.first == entering && p.second == candidate_leaving)) {
                        is_cycling = true;
                        break;
                    }
                }
                if (is_cycling && !use_bland && degenerate_steps >= 20) {
                    candidate_tried[entering] = true;
                    continue;
                }
                break;
            }
            if (phase == 1) {
                candidate_tried[entering] = true;
                continue;
            }
            for (double value : d) {
                if (value > 0) {
                    out.status = SolveStatus::numerical_failure;
                    out.iterations = step;
                    record_condition(factor, out);
                    return out;
                }
            }
            out.status = SolveStatus::unbounded;
            out.xb = std::move(xb);
            out.y = std::move(y);
            out.ray.assign(w.total_columns, 0);
            out.ray[entering] = 1;
            for (std::size_t i = 0; i < w.rows; ++i) {
                out.ray[w.basis[i]] = -d[i];
            }
            out.iterations = step;
            record_condition(factor, out);
            return out;
        }
        const auto leaving = w.basis[leaving_row];
        recent_pivots.push_back({entering, leaving});
        if (recent_pivots.size() > 16) {
            recent_pivots.pop_front();
        }
        if (log.size() < o.telemetry_limit) {
            log.push_back({log.size(), phase, dot(cb, xb), minimum_rc, entering, leaving,
                           theta <= o.feasibility_tolerance});
        } else {
            telemetry_truncated = true;
        }
        // Pivot with commit/rollback (numerical hygiene, cf. scsd1/scsd6):
        // the basis swap is only committed once the updated factorization is
        // known to be usable. A pivot whose new basis is numerically singular
        // must never survive: with the swap committed, both the in-place update
        // and the from-scratch rebuild fail identically and the exception
        // escaped the solver as NumericalFailure even though the previous basis
        // was perfectly good. On rejection the basis is restored and the
        // offending column is excluded from entering for the rest of this
        // phase, so the loop makes monotone progress instead of retrying the
        // same pivot forever.
        w.basis[leaving_row] = entering;
        bool pivot_committed = false;
        try {
            factor.replace_column(leaving_row, column(w, entering));
            if (factor.needs_refactorization())
                factor.refactorize();
            pivot_committed = true;
        } catch (const std::exception&) {
            try {
                factor = make_factor(w, s_opts);
                (void)factor.solve(w.b);
                pivot_committed = true;
            } catch (const std::exception&) {
                // New basis is singular: roll back.
            }
        }
        if (!pivot_committed) {
            w.basis[leaving_row] = leaving;
            rejected_column[entering] = 1;
            candidate_tried[entering] = true;
            // Rejecting an entering column can change the dual/basis state;
            // refactorize from the restored basis to be safe.
            try {
                factor = make_factor(w, s_opts);
            } catch (const std::exception&) {
                out.status = SolveStatus::numerical_failure;
                out.iterations = step;
                record_condition(factor, out);
                return out;
            }
        }
    }
    record_condition(factor, out);
    return out;
}
}

}
