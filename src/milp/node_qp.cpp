#include "markov_cero/milp/node_lp.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/verifier.hpp"
#include <algorithm>
#include <cmath>

namespace markov_cero::milp {
NodeLpResult solve_node_qp(const model::Model& model, const Options& options) {
    NodeLpResult result;
    try {
        auto q = qp::make_quadratic_model(model);
        q.variable_types.assign(q.num_variables(), model::VariableType::continuous);
        qp::QpOptions qo;
        qo.max_iterations = options.max_iterations;
        qo.absolute_tolerance = std::min(options.feasibility_tolerance, 1e-8);
        qo.relative_tolerance = qo.absolute_tolerance;
        qo.deadline = options.deadline;
        const auto sol = qp::solve_qp(q, qo);
        result.iterations = sol.iterations;
        result.condition_estimate = sol.condition_estimate;
        if (sol.status == qp::QpStatus::time_limit) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = "QP node deadline reached";
            return result;
        }
        if (sol.status == qp::QpStatus::primal_infeasible && qp::verify_qp_infeasibility(q, sol)) {
            result.status = lp::reference::SolveStatus::infeasible;
            result.message = "QP node Farkas witness verified";
            return result;
        }
        const auto report = qp::verify_qp_solution(q, sol, std::min(1e-4, std::max(1e-7, options.feasibility_tolerance)));
        result.status = lp::reference::SolveStatus::numerical_failure;
        if (sol.status != qp::QpStatus::optimal || !report.passed) {
            result.message = "QP node lacks a verified KKT witness: " + report.failure_reason;
            return result;
        }
        result.lower_bound = qp::supporting_lower_bound(model, q, sol);
        if (!std::isfinite(result.lower_bound)) {
            result.status = lp::reference::SolveStatus::unsupported;
            result.message = "QP node supporting bound has an infinite box infimum";
            return result;
        }
        result.status = lp::reference::SolveStatus::optimal;
        result.primal = sol.x;
        result.objective = sol.objective_value;
        result.message = "convex QP node KKT checked; supporting-hyperplane box bound";
    } catch (const std::exception& error) {
        result.status = lp::reference::SolveStatus::numerical_failure;
        result.message = error.what();
    }
    return result;
}
} // namespace markov_cero::milp
