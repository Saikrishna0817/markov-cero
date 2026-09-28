#include "mip_proof_internal.hpp"
#include "markov_cero/qp/verifier.hpp"
namespace markov_cero::verify::mip_detail {
namespace {
qp::QuadraticModel quadratic(const model::Model& model) {
    auto q = qp::make_quadratic_model(model);
    q.variable_types.assign(q.num_variables(), model::VariableType::continuous);
    return q;
}
qp::QpSolution quadratic_witness(const lp::reference::Result& witness) {
    qp::QpSolution sol;
    sol.status = witness.status == lp::reference::SolveStatus::optimal
        ? qp::QpStatus::optimal : qp::QpStatus::primal_infeasible;
    sol.x = witness.primal; sol.y = witness.dual;
    sol.infeasibility_certificate = witness.certificate;
    sol.objective_value = witness.objective;
    return sol;
}
}
double check_relaxation(const model::Model& model, const lp::reference::Result& witness,
                        const MipProofOptions& options) {
    if (model.has_quadratic_objective) {
        const auto q = quadratic(model);
        const auto sol = quadratic_witness(witness);
        if (witness.status == lp::reference::SolveStatus::infeasible) {
            if (!qp::verify_qp_infeasibility(q, sol, options.tolerance))
                throw std::invalid_argument("invalid QP Farkas leaf");
            return std::numeric_limits<double>::infinity();
        }
        const auto convexity = qp::assess_convexity(q.P, 1e-10, 5U*1024U*1024U, options.deadline);
        if (convexity.deadline_reached || convexity.status != qp::ConvexityStatus::positive_semidefinite ||
            !qp::verify_qp_solution(q, sol, options.tolerance).passed)
            throw std::invalid_argument("invalid convex QP KKT leaf");
        return qp::supporting_lower_bound(model, q, sol);
    }
    const auto canonical = transform::sparse_canonicalize(model, true);
    if (!verify_sparse_result(canonical, witness, options.tolerance).accepted)
        throw std::invalid_argument("invalid LP leaf relaxation witness");
    if (witness.status == lp::reference::SolveStatus::infeasible)
        return std::numeric_limits<double>::infinity();
    long double bound = canonical.objective_offset;
    for (std::size_t i=0; i<canonical.rhs.size(); ++i)
        bound += static_cast<long double>(canonical.rhs[i]) * witness.dual[i];
    return static_cast<double>(bound);
}
Relaxation solve_relaxation(const model::Model& model, const MipProofOptions& options) {
    Relaxation out;
    if (model.has_quadratic_objective) {
        auto q = quadratic(model);
        qp::QpOptions settings;
        settings.deadline = options.deadline;
        settings.absolute_tolerance = settings.relative_tolerance = options.tolerance * .1;
        const auto sol = qp::solve_qp(q, settings);
        out.witness.status = sol.status == qp::QpStatus::optimal ? lp::reference::SolveStatus::optimal
            : sol.status == qp::QpStatus::primal_infeasible ? lp::reference::SolveStatus::infeasible
            : lp::reference::SolveStatus::numerical_failure;
        out.witness.primal = sol.x; out.witness.dual = sol.y;
        out.witness.certificate = sol.infeasibility_certificate;
        out.witness.objective = sol.objective_value;
        out.primal = sol.x;
    } else {
        const auto canonical = transform::sparse_canonicalize(model, true);
        lp::reference::Options settings;
        settings.deadline = options.deadline;
        settings.bland_anti_cycling = false;
        out.witness = lp::reference::solve(canonical, settings);
        out.witness.telemetry.clear(); out.witness.basis.clear();
        if (out.witness.status == lp::reference::SolveStatus::optimal)
            out.primal = transform::reconstruct_primal(canonical, out.witness.primal);
    }
    if (out.witness.status == lp::reference::SolveStatus::optimal ||
        out.witness.status == lp::reference::SolveStatus::infeasible)
        out.bound = check_relaxation(model, out.witness, options);
    return out;
}
} // namespace markov_cero::verify::mip_detail
