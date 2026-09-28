#include "markov_cero/milp/node_lp.hpp"

#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "markov_cero/qp/model.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

namespace markov_cero::milp {

namespace {
constexpr std::size_t kNodeLpIterationCap = 50000;
// The reference simplex and dual warm-start interfaces still consume a dense
// tableau. Refuse over-limit models explicitly before allocating rows*cols.
constexpr std::size_t kDenseNodeLpMaxRows = 4096;
constexpr std::size_t kDenseNodeLpMaxColumns = 16384;
constexpr std::size_t kSparseNodeLpIterationCap = 10000;

/// R13/R17: certify a node LP answer before the branch-and-cut trusts it.
/// A node bound is used to *prove* optimality, so an uncertified optimum is
/// strictly worse than an admitted failure: it silently prunes the true
/// optimum. This is the same primal/dual check the API-level verifiers apply
/// (Ax = b and c - A^T y >= 0 for the canonical min form), run per node LP
/// because the MILP path bypasses those verifiers.
struct WitnessViolations {
    double primal{0.0};
    double dual{0.0};
    bool sized{false};
};

[[nodiscard]] WitnessViolations witness_violations(const transform::SparseCanonicalModel& canon,
                                                   const std::vector<double>& primal,
                                                   const std::vector<double>& dual) {
    WitnessViolations out;
    if (primal.size() != canon.matrix.columns || dual.size() != canon.matrix.rows)
        return out;
    const auto ax = canon.multiply(primal);
    const auto aty = canon.multiply_transpose(dual);
    if (ax.size() != canon.rhs.size() || aty.size() != canon.objective.size())
        return out;
    out.sized = true;
    for (std::size_t i = 0; i < canon.rhs.size(); ++i)
        out.primal = std::max(out.primal, std::abs(ax[i] - canon.rhs[i]));
    for (std::size_t j = 0; j < canon.objective.size(); ++j)
        out.dual = std::max(out.dual, -(canon.objective[j] - aty[j]));
    return out;
}

[[nodiscard]] bool canonical_witness_certified(
    const transform::SparseCanonicalModel& canon, const std::vector<double>& primal,
    const std::vector<double>& dual, double primal_tolerance, double dual_tolerance) {
    const auto v = witness_violations(canon, primal, dual);
    double max_b = 1.0;
    for (double b_val : canon.rhs) {
        max_b = std::max(max_b, std::abs(b_val));
    }
    double max_c = 1.0;
    for (double c_val : canon.objective) {
        max_c = std::max(max_c, std::abs(c_val));
    }
    return v.sized && v.primal <= primal_tolerance * max_b && v.dual <= dual_tolerance * max_c;
}

[[nodiscard]] std::vector<double> reconstruct_row_duals(
    const transform::SparseCanonicalModel& canon,
    const std::vector<double>& canonical_dual) {
    std::vector<double> out(canon.record.rows.size(), 0.0);
    for (std::size_t i = 0; i < canon.record.rows.size(); ++i) {
        const auto& map = canon.record.rows[i];
        for (std::size_t k = 0; k < map.canonical_index.size(); ++k) {
            const std::size_t idx = map.canonical_index[k];
            if (idx >= canonical_dual.size() || k >= map.multiplier.size()) return {};
            out[i] += canon.record.objective_sign * map.multiplier[k] * canonical_dual[idx];
        }
    }
    if (!std::all_of(out.begin(), out.end(), [](double y) { return std::isfinite(y); })) return {};
    return out;
}

// A PDLP iterate can provide a weak-duality lower bound even before
// convergence, but only if its row multipliers obey the row-bound normal
// cones and c + A^T y has a finite infimum over every variable interval.
// PDLP's reported dual objective is an approximate KKT quantity and omits
// infinite-endpoint terms, so branch-and-bound independently checks finiteness.
[[nodiscard]] bool has_finite_lagrangian_infimum(
    const model::Model& mdl, const std::vector<double>& dual) {
    if (dual.size() != mdl.matrix.row_count || mdl.objective.size() != mdl.matrix.column_count)
        return false;
    for (std::size_t i = 0; i < dual.size(); ++i) {
        if (!std::isfinite(dual[i])) return false;
        if (dual[i] > 0.0 && !mdl.row_upper[i].is_finite()) return false;
        if (dual[i] < 0.0 && !mdl.row_lower[i].is_finite()) return false;
    }
    std::vector<double> reduced = mdl.objective;
    for (std::size_t j = 0; j < mdl.matrix.column_count; ++j) {
        for (std::size_t p = mdl.matrix.column_start[j];
             p < mdl.matrix.column_start[j + 1]; ++p) {
            reduced[j] += mdl.matrix.value[p] * dual[mdl.matrix.row_index[p]];
        }
    }
    for (std::size_t j = 0; j < reduced.size(); ++j) {
        if (!std::isfinite(reduced[j])) return false;
        if (reduced[j] > 0.0 && !mdl.variable_lower[j].is_finite()) return false;
        if (reduced[j] < 0.0 && !mdl.variable_upper[j].is_finite()) return false;
    }
    return true;
}

/// Debug hook consistent with the repo's existing `MARKOV_RW2_DEBUG` pattern:
/// `MARKOV_NODE_DEBUG=1 markov-cero-solve m.mps --engine milp` explains why a
/// node LP was rejected.
void debug_node_lp(const char* stage, const char* detail,
                   const WitnessViolations& v = {}) {
    if (!std::getenv("MARKOV_NODE_DEBUG"))
        return;
    std::fprintf(stderr, "[node-lp] %s: %s (primal_viol=%.3e dual_viol=%.3e)\n", stage, detail,
                 v.primal, v.dual);
}
}

NodeLpResult solve_node_relaxation(
    const model::Model& node_model,
    const Options& options,
    const std::optional<lp::dual::BasisState>& warm_start) {
    NodeLpResult res;

    if (node_model.has_quadratic_objective) {
        try {
            const auto qp = qp::make_quadratic_model(node_model);
            qp::QpOptions qopts;
            qopts.max_iterations = options.max_iterations;
            qopts.absolute_tolerance = options.feasibility_tolerance;
            qopts.relative_tolerance = options.feasibility_tolerance;
            qopts.deadline = options.deadline;
            const auto qpres = qp::solve_qp(qp, qopts);
            res.condition_estimate = qpres.condition_estimate;

            if (qpres.status == qp::QpStatus::optimal) {
                res.status = lp::reference::SolveStatus::optimal;
                res.primal = qpres.x;
                for (std::size_t j = 0; j < res.primal.size(); ++j) {
                    const auto& lo = node_model.variable_lower[j];
                    const auto& up = node_model.variable_upper[j];
                    if (lo.is_finite() && res.primal[j] < lo.value) res.primal[j] = lo.value;
                    if (up.is_finite() && res.primal[j] > up.value) res.primal[j] = up.value;
                }
                res.objective = qpres.objective_value;
                res.lower_bound = qpres.objective_value;
                res.iterations = qpres.iterations;
            } else if (qpres.status == qp::QpStatus::primal_infeasible) {
                res.status = lp::reference::SolveStatus::infeasible;
            } else if (qpres.status == qp::QpStatus::time_limit) {
                res.status = lp::reference::SolveStatus::resource_limit;
                res.message = "QP node relaxation wall-clock deadline reached";
            } else {
                res.status = lp::reference::SolveStatus::numerical_failure;
            }
        } catch (...) {
            res.status = lp::reference::SolveStatus::numerical_failure;
        }
        return res;
    }

    try {
        const auto canon =
            transform::sparse_canonicalize(node_model, /*relax_integrality=*/true);
        if (canon.matrix.rows > kDenseNodeLpMaxRows ||
            canon.matrix.columns > kDenseNodeLpMaxColumns) {
            if (node_model.has_quadratic_objective ||
                node_model.objective_sense != model::ObjectiveSense::minimize) {
                res.status = lp::reference::SolveStatus::unsupported;
                res.message = "large node relaxation is outside the sparse PDLP "
                              "fallback scope (linear minimization only)";
                return res;
            }
            lp::first_order::PdlpOptions popts;
            // The outer MILP time limit is checked between node LPs. Bound a
            // single first-order relaxation so a difficult root cannot ignore
            // the caller's time budget for hundreds of thousands of steps.
            popts.max_iterations = std::min(options.max_iterations,
                                            kSparseNodeLpIterationCap);
            popts.set_tolerance(std::min(options.feasibility_tolerance, 1e-7));
            popts.deadline = options.deadline;
            const auto pres = lp::first_order::solve_pdlp(node_model, popts);
            if (pres.dual.size() == node_model.matrix.row_count) res.row_dual = pres.dual;
            res.iterations = pres.iterations;
            res.condition_estimate = pres.condition_estimate;
            const bool finite_dual_bound =
                std::isfinite(pres.dual_objective) &&
                has_finite_lagrangian_infimum(node_model, pres.dual);
            if (finite_dual_bound) {
                const double roundoff_guard =
                    1e-10 * (1.0 + std::abs(pres.dual_objective));
                res.lower_bound = pres.dual_objective - roundoff_guard;
            }
            if (pres.status != lp::first_order::PdlpStatus::optimal ||
                pres.primal.size() != node_model.matrix.column_count ||
                !std::isfinite(pres.dual_objective) ||
                pres.primal_infeasibility > popts.primal_tolerance ||
                pres.dual_infeasibility > popts.dual_tolerance ||
                pres.duality_gap > popts.gap_tolerance || !finite_dual_bound) {
                res.status = pres.status == lp::first_order::PdlpStatus::resource_limit
                                 ? lp::reference::SolveStatus::resource_limit
                                 : lp::reference::SolveStatus::iteration_limit;
                res.message = !finite_dual_bound
                                  ? "sparse PDLP iterate has no finite verified weak-dual bound"
                                  : "sparse PDLP node relaxation did not meet its primal, "
                                        "dual, and gap tolerances: " + pres.message;
                return res;
            }
            res.status = lp::reference::SolveStatus::optimal;
            res.primal = pres.primal;
            res.objective = node_model.objective_offset;
            for (std::size_t j = 0; j < res.primal.size(); ++j)
                res.objective += node_model.objective[j] * res.primal[j];
            // PDLP's dual objective is a valid weak-duality bound for every
            // iterate (row multipliers remain in the row-bound normal cone).
            // Move it slightly downward to cover ordinary floating summation
            // error before using it to prune a branch.
            res.message = "sparse PDLP relaxation converged; using conservative dual bound";
            return res;
        }
        const auto dense = canon.to_dense();

        // Certification slack: generous enough not to reject legitimately
        // solved degenerate LPs, tight enough to catch a wrong optimum.
        const double primal_tol = std::max(10.0 * options.feasibility_tolerance, 1e-6);
        const double dual_tol = primal_tol;

        if (options.enable_warm_start && warm_start.has_value()) {
            lp::dual::Options dopts;
            dopts.iteration_limit = std::min(options.max_iterations, kNodeLpIterationCap);
            dopts.feasibility_tolerance = options.feasibility_tolerance;
            dopts.allow_cold_fallback = true;
            dopts.deadline = options.deadline;
            const auto dres = lp::dual::solve(dense, dopts, warm_start);
            res.status = dres.solution.status;
            res.iterations =
                dres.solution.phase_one_iterations + dres.solution.phase_two_iterations;
            res.condition_estimate = dres.solution.condition_estimate;
            if (res.status == lp::reference::SolveStatus::optimal &&
                canonical_witness_certified(canon, dres.solution.primal, dres.solution.dual,
                                            primal_tol, dual_tol)) {
                res.primal = transform::reconstruct_primal(canon, dres.solution.primal);
                    res.objective =
                        transform::reconstruct_objective(canon, dres.solution.objective);
                res.lower_bound = res.objective;
                res.basis = dres.basis_state;
                res.row_dual = reconstruct_row_duals(canon, dres.solution.dual);
            } else if (res.status == lp::reference::SolveStatus::optimal) {
                // Uncertified optimum (warm-started dual simplex on a badly
                // conditioned/degenerate node LP): retry cold with the
                // reference primal simplex, which is the robust path.
                debug_node_lp("warm-dual uncertified", "retrying cold",
                              witness_violations(canon, dres.solution.primal,
                                                 dres.solution.dual));
                lp::reference::Options ropts;
                ropts.iteration_limit = options.max_iterations;
                ropts.feasibility_tolerance = options.feasibility_tolerance;
                ropts.bland_anti_cycling = false;
                ropts.deadline = options.deadline;
                const auto rres = lp::reference::solve(dense, ropts);
                res.iterations += rres.phase_one_iterations + rres.phase_two_iterations;
                if (rres.condition_estimate > 0.0) {
                    res.condition_estimate = rres.condition_estimate;
                }
                if (rres.status == lp::reference::SolveStatus::optimal &&
                    canonical_witness_certified(canon, rres.primal, rres.dual, primal_tol,
                                                dual_tol)) {
                    res.status = lp::reference::SolveStatus::optimal;
                    res.primal = transform::reconstruct_primal(canon, rres.primal);
                    res.objective = transform::reconstruct_objective(canon, rres.objective);
                    res.lower_bound = res.objective;
                    res.row_dual = reconstruct_row_duals(canon, rres.dual);
                    res.basis.reset();
                    if (rres.basis.size() == dense.matrix.rows) {
                        try {
                            res.basis = lp::dual::make_basis_state(dense, rres.basis);
                        } catch (...) {
                        }
                    }
                } else {
                    // Do not hand an uncertified bound upward.
                    debug_node_lp("cold primal uncertified", "rejecting node LP",
                                  witness_violations(canon, rres.primal, rres.dual));
                    res.status = lp::reference::SolveStatus::numerical_failure;
                    res.primal.clear();
                    res.objective = 0.0;
                    res.basis.reset();
                }
            }
        } else {
            lp::reference::Options ropts;
            ropts.iteration_limit = options.max_iterations;
            ropts.feasibility_tolerance = options.feasibility_tolerance;
            ropts.bland_anti_cycling = false;
            ropts.deadline = options.deadline;
            const auto rres = lp::reference::solve(dense, ropts);
            res.status = rres.status;
            res.iterations = rres.phase_one_iterations + rres.phase_two_iterations;
            res.condition_estimate = rres.condition_estimate;
            if (res.status != lp::reference::SolveStatus::optimal) {
                debug_node_lp("cold primal non-optimal", rres.message.c_str());
            }
            if (res.status == lp::reference::SolveStatus::optimal) {
                if (!canonical_witness_certified(canon, rres.primal, rres.dual, primal_tol,
                                                 dual_tol)) {
                    debug_node_lp("cold primal uncertified", "rejecting node LP",
                                  witness_violations(canon, rres.primal, rres.dual));
                    res.status = lp::reference::SolveStatus::numerical_failure;
                    res.primal.clear();
                    res.objective = 0.0;
                } else {
                    res.primal = transform::reconstruct_primal(canon, rres.primal);
                    res.objective = transform::reconstruct_objective(canon, rres.objective);
                    res.lower_bound = res.objective;
                    res.row_dual = reconstruct_row_duals(canon, rres.dual);
                    if (rres.basis.size() == dense.matrix.rows) {
                        try {
                            res.basis = lp::dual::make_basis_state(dense, rres.basis);
                        } catch (...) {
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        if (std::getenv("MARKOV_NODE_DEBUG")) {
            std::fprintf(stderr, "[node-lp] EXCEPTION: %s\n", e.what());
        }
        res.status = lp::reference::SolveStatus::numerical_failure;
        res.message = e.what();
    } catch (...) {
        if (std::getenv("MARKOV_NODE_DEBUG")) {
            std::fprintf(stderr, "[node-lp] UNKNOWN EXCEPTION\n");
        }
        res.status = lp::reference::SolveStatus::numerical_failure;
        res.message = "unknown exception while solving node LP";
    }
    return res;
}

} // namespace markov_cero::milp
