#include "markov_cero/milp/node_lp.hpp"

#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"

#include "markov_cero/verify/reference_lp_verifier.hpp"
#include "markov_cero/verify/linear_certificate.hpp"
#include "markov_cero/qp/verifier.hpp"
#include <cmath>
#include <algorithm>
#include "markov_cero/qp/model.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

namespace markov_cero::milp {

namespace {
constexpr std::size_t kNodeLpIterationCap = 50000;
// Larger nodes use bounded first-order iterations; revised simplex is sparse.
// Only small warm-start tableaux use the separate dense compatibility adapter.
constexpr std::size_t kRevisedNodeLpMaxRows = 4096;
constexpr std::size_t kRevisedNodeLpMaxColumns = 16384;
constexpr std::size_t kSparseNodeLpIterationCap = 10000;

void certify_result(const transform::SparseCanonicalModel& model,
                    lp::reference::Result& result, double tolerance) {
    using Status = lp::reference::SolveStatus;
    if (result.status != Status::optimal && result.status != Status::infeasible &&
        result.status != Status::unbounded) return;
    const auto report = verify::verify_sparse_result(model, result, tolerance);
    if (!report.accepted) {
        result.status = Status::numerical_failure;
        result.message = "node witness rejected: " + report.message;
    }
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


}

NodeLpResult solve_node_relaxation(
    const model::Model& node_model,
    const Options& options,
    const std::optional<lp::dual::BasisState>& warm_start) {
    NodeLpResult res;

    if (node_model.has_quadratic_objective)
        return solve_node_qp(node_model, options);

    try {
        const auto canon =
            transform::sparse_canonicalize(node_model, /*relax_integrality=*/true);
        if (canon.matrix.rows > kRevisedNodeLpMaxRows ||
            canon.matrix.columns > kRevisedNodeLpMaxColumns) {
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
            const auto certificate = verify::verify_linear_solution(node_model, pres.primal,
                pres.dual, pres.objective, popts.gap_tolerance, true);
            const bool finite_dual_bound = std::isfinite(certificate.bound);
            if (finite_dual_bound)
                res.lower_bound = certificate.bound - 1e-10 * (1.0 + std::abs(certificate.bound));
            if (pres.status != lp::first_order::PdlpStatus::optimal || !certificate.accepted) {
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
        const bool small_basis = canon.matrix.columns == 0 || canon.matrix.rows <= 262144 / canon.matrix.columns;
        transform::CanonicalModel dense;
        if (small_basis) dense = canon.to_dense();

        // Certification slack: generous enough not to reject legitimately
        // solved degenerate LPs, tight enough to catch a wrong optimum.
        const double primal_tol = std::min(1e-4, std::max(options.feasibility_tolerance, 1e-8));

        if (small_basis && small_basis && options.enable_warm_start && warm_start.has_value()) {
            lp::dual::Options dopts;
            dopts.iteration_limit = std::min(options.max_iterations, kNodeLpIterationCap);
            dopts.feasibility_tolerance = options.feasibility_tolerance;
            dopts.allow_cold_fallback = true;
            dopts.deadline = options.deadline;
            auto dres = lp::dual::solve(dense, dopts, warm_start);
            certify_result(canon, dres.solution, primal_tol);
            res.status = dres.solution.status;
            res.iterations =
                dres.solution.phase_one_iterations + dres.solution.phase_two_iterations;
            res.condition_estimate = dres.solution.condition_estimate;
            if (res.status == lp::reference::SolveStatus::optimal) {
                res.primal = transform::reconstruct_primal(canon, dres.solution.primal);
                    res.objective =
                        transform::reconstruct_objective(canon, dres.solution.objective);
                res.lower_bound = res.objective;
                res.basis = dres.basis_state;
                res.row_dual = reconstruct_row_duals(canon, dres.solution.dual);
            } else if (res.status == lp::reference::SolveStatus::numerical_failure) {
                // Uncertified optimum (warm-started dual simplex on a badly
                // conditioned/degenerate node LP): retry cold with the
                // reference primal simplex, which is the robust path.
                lp::reference::Options ropts;
                ropts.iteration_limit = options.max_iterations;
                ropts.feasibility_tolerance = options.feasibility_tolerance;
                ropts.bland_anti_cycling = false;
                ropts.deadline = options.deadline;
                auto rres = lp::reference::solve(canon, ropts);
                certify_result(canon, rres, primal_tol);
                res.iterations += rres.phase_one_iterations + rres.phase_two_iterations;
                if (rres.condition_estimate > 0.0) {
                    res.condition_estimate = rres.condition_estimate;
                }
                if (rres.status == lp::reference::SolveStatus::optimal) {
                    res.status = lp::reference::SolveStatus::optimal;
                    res.primal = transform::reconstruct_primal(canon, rres.primal);
                    res.objective = transform::reconstruct_objective(canon, rres.objective);
                    res.lower_bound = res.objective;
                    res.row_dual = reconstruct_row_duals(canon, rres.dual);
                    res.basis.reset();
                    if (small_basis && rres.basis.size() == dense.matrix.rows) {
                        try {
                            res.basis = lp::dual::make_basis_state(dense, rres.basis);
                        } catch (...) {
                        }
                    }
                } else {
                    // Do not hand an uncertified bound upward.
                    res.status = rres.status;
                    res.message = rres.message;
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
            auto rres = lp::reference::solve(canon, ropts);
                certify_result(canon, rres, primal_tol);
            res.status = rres.status;
            res.message = rres.message;
            res.iterations = rres.phase_one_iterations + rres.phase_two_iterations;
            res.condition_estimate = rres.condition_estimate;
            if (res.status == lp::reference::SolveStatus::optimal) {
                    res.primal = transform::reconstruct_primal(canon, rres.primal);
                    res.objective = transform::reconstruct_objective(canon, rres.objective);
                    res.lower_bound = res.objective;
                    res.row_dual = reconstruct_row_duals(canon, rres.dual);
                    if (small_basis && rres.basis.size() == dense.matrix.rows) {
                        try {
                            res.basis = lp::dual::make_basis_state(dense, rres.basis);
                        } catch (...) {
                        }
                    }
            }
        }
    } catch (const std::exception& e) {
        res.status = lp::reference::SolveStatus::numerical_failure;
        res.message = e.what();
    } catch (...) {
        res.status = lp::reference::SolveStatus::numerical_failure;
        res.message = "unknown exception while solving node LP";
    }
    return res;
}

} // namespace markov_cero::milp
