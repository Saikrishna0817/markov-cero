#include "revised_simplex_internal.hpp"

namespace markov_cero::lp::reference {
using namespace detail_revised_simplex;

namespace detail_revised_simplex {
double inf_norm(const std::vector<double>& v) {
    double m = 0.0;
    for (double x : v) m = std::max(m, std::abs(x));
    return m;
}
// Drift detector gate: an intermediate-step solve residual above 1e-6 scaled
// by the right-hand side means iterative refinement could not represent the
// solution through the current eta chain (or the basis itself is near
// singular). That is orders of magnitude above solver noise (refinement
// targets 1e-14) and far below the final witness bars, so it never
// substitutes for certification — it only triggers a fresh factorization (or
// a phase-I restart) before numerical drift poisons the pivot sequence the
// way it did on bore3d/scsd1/scsd6/etamacro/e226 (degenerate phase I stalls
// whose dual vectors exploded to 1e15 while the incremental factorization
// still reported healthy).
double drift_gate(double rhs_scale) {
    return 1e-6 * std::max(1.0, rhs_scale);
}
}

namespace detail_revised_simplex {
bool recover_step(Work& w, linalg::SparseBasisFactorization& factor,
                  const linalg::SparseBasisOptions& s_opts, int phase,
                  bool& artificial_reset_used, IterationOutcome& out, std::size_t step,
                  bool& restart, const std::string& why) {
    if (phase != 1 || artificial_reset_used) {
        out.status = SolveStatus::numerical_failure;
        out.iterations = step;
        out.message = why;
        record_condition(factor, out);
        return false;
    }
    artificial_reset_used = true;
    try {
        for (std::size_t i = 0; i < w.rows; ++i)
            w.basis[i] = w.original_columns + i;
        factor = make_factor(w, s_opts);
    } catch (const std::exception& e) {
        out.status = SolveStatus::numerical_failure;
        out.iterations = step;
        out.message = why + "; artificial basis reset failed: " + e.what();
        record_condition(factor, out);
        return false;
    }
    restart = true;
    return true;
}
}

namespace detail_revised_simplex {
PivotAction price_and_trial(Work& w, const std::vector<double>& cost,
                            const std::vector<double>& y, std::vector<double>& xb,
                            const std::vector<bool>& basic, const Options& o, int phase,
                            std::size_t step, bool use_bland, std::size_t enter_limit,
                            linalg::SparseBasisFactorization& factor,
                            const linalg::SparseBasisOptions& s_opts, double b_scale,
                            std::vector<char>& rejected_column,
                            std::vector<unsigned char>& reject_revivals,
                            std::vector<bool>& candidate_tried, IterationOutcome& out,
                            bool& artificial_reset_used, bool& restart) {
    std::size_t entering = enter_limit;
    std::size_t leaving_row = w.rows;
    std::size_t ejected = w.rows;
    std::vector<double> d;
    double theta = 0;
    double minimum_rc = 0;
    constexpr unsigned kMaxRejectRevivals = 3;
    PivotAction action;
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
                long double art = 0;
                if (phase == 1) {
                    for (std::size_t i = 0; i < w.rows; ++i)
                        if (w.basis[i] >= w.original_columns) art += xb[i];
                }
                // Excluding a column is a within-step progress heuristic; it
                // must never manufacture an optimum. An excluded nonbasic
                // column whose reduced cost is still negative proves pricing
                // lied (bore3d col 40, e226 col 314: phase I "optimal" with
                // artificials basic and huge negative rc parked behind the
                // exclusion). Either revive it — a later basis state may
                // pivot it successfully — or fail honestly once it has
                // burned its bounded revivals. In phase I with artificials
                // already driven to feasibility the phase is a genuine
                // success regardless of parked columns (their optimality is
                // phase II's job).
                const bool suspect_optimality =
                    phase != 1 || art > o.feasibility_tolerance;
                if (suspect_optimality) {
                    bool revived = false;
                    for (std::size_t j = 0; j < enter_limit; ++j) {
                        if ((!rejected_column[j] && !candidate_tried[j]) || basic[j])
                            continue;
                        const double rc = cost[j] - column_dot(w, j, y);
                        if (!significant_negative_reduced_cost(w, j, cost, y, rc,
                                                              o.dual_tolerance))
                            continue;
                        if (reject_revivals[j] < kMaxRejectRevivals) {
                            ++reject_revivals[j];
                            rejected_column[j] = 0;
                            candidate_tried[j] = false;
                            revived = true;
                            continue;
                        }
                        out.status = SolveStatus::numerical_failure;
                        out.iterations = step;
                        out.message =
                            phase == 1
                                ? "phase I excluded column " + std::to_string(j) +
                                      " whose reduced cost is still negative"
                                : "phase II excluded column " + std::to_string(j) +
                                      " whose reduced cost is still negative";
                        record_condition(factor, out);
                        return PivotAction::of(PivotAction::finished);
                    }
                    if (revived) continue;
                }
                out.status = SolveStatus::optimal;
                out.xb = std::move(xb);
                out.y = std::move(y);
                out.iterations = step;
                record_condition(factor, out);
                return PivotAction::of(PivotAction::finished);
            }
            d = factor.solve(column(w, entering));
            leaving_row = select_leaving(w, xb, d, o, theta);
            if (leaving_row < w.rows) {
                // Degenerate tie retry: every row at the minimum theta is a
                // valid blocking row, and the primary choice may form a basis
                // no ordering factorizes (bore3d's col 40, rc -76, was
                // excluded forever after one failed trial, stalling phase I
                // with artificials still in the basis). Try each tied row
                // before excluding the entering column from this phase.
                std::vector<std::size_t> tie_rows;
                tie_rows.push_back(leaving_row);
                for (std::size_t i = 0; i < w.rows; ++i) {
                    if (i == leaving_row || d[i] <= 0.0) continue;
                    if (xb[i] / d[i] == theta) tie_rows.push_back(i);
                }
                bool trial_committed = false;
                // A basis this solver accepts must SOLVE cleanly, not just
                // factorize: bore3d's phase I committed bases whose fresh
                // LU passed the singular floor yet whose transpose solves
                // carried y-components of 1e15 (effective cond ~1e15 while
                // the condition proxy reported 5e10). Checking both solve
                // directions at pivot time lets the tie retry fall through
                // to an alternative blocking row instead of poisoning the
                // next step, where recovery could only restart the same
                // deterministic path.
                const auto basis_solves_cleanly = [&]() {
                    try {
                        std::vector<double> c_B(w.rows);
                        for (std::size_t i = 0; i < w.rows; ++i)
                            c_B[i] = cost[w.basis[i]];
                        const auto y_probe = factor.solve_transpose(c_B);
                        if (linalg::sparse_infinity_residual(factor.current_basis(), y_probe,
                                                             c_B, /*transpose=*/true) >
                            drift_gate(inf_norm(c_B)))
                            return false;
                        const auto x_probe = factor.solve(w.b);
                        if (linalg::sparse_infinity_residual(factor.current_basis(), x_probe,
                                                             w.b) > drift_gate(b_scale))
                            return false;
                        // Feasibility at the tolerance snap_basic_solution
                        // will demand next step: a trial basis the ratio test
                        // deems feasible can still re-solve as infeasible past
                        // that bar when the factorization is ill-conditioned
                        // (bore3d died silently on exactly this after a
                        // revival pivot). Reject the trial here so the tie
                        // retry can pick another blocking row.
                        double x_norm = 1.0;
                        for (double v : x_probe) x_norm = std::max(x_norm, std::abs(v));
                        const double snap_tol = std::max({o.feasibility_tolerance, 1e-6,
                                                          1e-8 * x_norm});
                        for (double v : x_probe)
                            if (v < -snap_tol) return false;
                        return true;
                    } catch (const std::exception&) {
                        return false;
                    }
                };
                for (std::size_t row : tie_rows) {
                    const auto previous = w.basis[row];
                    w.basis[row] = entering;
                    bool committed = false;
                    try {
                        factor.replace_column(row, column(w, entering));
                        // Validate the committed basis through the solve gates
                        // below (fresh LU only at the update/density triggers):
                        // an unconditional refactorize here cost a full sparse
                        // LU per pivot and regressed LP-heavy Debug/TSan runs
                        // 3x over the trigger-only baseline, while the
                        // basis_solves_cleanly residual checks still reject the
                        // eta-chain singularities (bore3d col 40) that a fresh
                        // LU would have caught.
                        if (factor.needs_refactorization()) factor.refactorize();
                        committed = true;
                    } catch (const std::exception&) {
                        try {
                            factor = make_factor(w, s_opts);
                            (void)factor.solve(w.b);
                            committed = true;
                        } catch (const std::exception&) {
                            // Singular trial basis: restore and try the next
                            // tied row.
                        }
                    }
                    if (committed && !basis_solves_cleanly()) committed = false;
                    if (committed) {
                        trial_committed = true;
                        ejected = previous;
                        leaving_row = row;
                        break;
                    }
                    w.basis[row] = previous;
                    try {
                        factor = make_factor(w, s_opts);
                    } catch (const std::exception& e) {
                        if (!recover_step(w, factor, s_opts, phase, artificial_reset_used,
                                  out, step, restart,
                                  std::string("accepted basis no longer factorizes: ") +
                                     e.what()))
                            return PivotAction::of(PivotAction::finished);
                        break;
                    }
                }
                if (restart) break;
                if (!trial_committed) {
                    // Every tied blocking row formed a singular basis: exclude
                    // the column (monotone progress) and price another entrant
                    // this step.
                    rejected_column[entering] = 1;
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
                    out.message = "positive direction component in phase II pricing";
                    record_condition(factor, out);
                    return PivotAction::of(PivotAction::finished);
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
            return PivotAction::of(PivotAction::finished);
    }
    if (restart) return PivotAction::of(PivotAction::restart);
    action.kind = PivotAction::pivot;
    action.entering = entering;
    action.ejected = ejected;
    action.theta = theta;
    action.minimum_rc = minimum_rc;
    return action;
}
}
}
