// W1 gate: convex MINLP via outer approximation.
//   minimize  x0^2 + x1^2
//   s.t.      x0 + x1 >= 1.5   (x1 integer)
//   -> integer optimum (sqrt rounding): x0 = 0.5, x1 = 1, obj = 1.25.
#include "markov_cero/minlp/minlp_solver.hpp"
#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/nlp/nlp_model.hpp"

#include <cmath>
#include <chrono>
#include <cstdlib>
#include <iostream>

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

} // namespace

int main() {
    model::Model source;
    source.name = "minlp_basic";
    model::SparseMatrixBuilder a(1, 2);
    a.add(0, 0, 1.0);
    a.add(0, 1, 1.0);
    source.matrix = a.build();
    source.objective = {0.0, 0.0};
    source.row_name = {"demand"};
    source.row_lower = {model::Bound::finite(1.5)};
    source.row_upper = {model::Bound::positive_infinity()};
    source.variable_name = {"x0", "x1"};
    source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(3.0), model::Bound::finite(3.0)};
    source.variable_type = {model::VariableType::continuous, model::VariableType::integer};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{1.0, 0, 0, true}, {1.0, 1, 1, true}};
    source.validate();

    minlp::MinlpProblem problem;
    problem.source_model = source;
    problem.nlp = io::make_nlp_model(source);
    problem.integer_indices = {1};  // x1 integer

    minlp::MinlpOptions opts;
    const auto sol = minlp::solve_minlp(problem, {0.2, 0.2}, opts);
    req(sol.status == lp::reference::SolveStatus::optimal, "minlp reaches optimal");
    req(sol.integer_feasible, "minlp solution is integer feasible");
    req(std::abs(sol.x[1] - std::round(sol.x[1])) < 1e-6, "x1 integral");
    req(std::abs(sol.objective - 1.25) < 1e-3, "objective reaches 1.25 (0.5,1)");
    req(sol.x[0] + sol.x[1] >= 1.5 - 1e-6, "constraint satisfied");
    auto expired_opts = opts;
    expired_opts.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto expired_sol = minlp::solve_minlp(problem, {0.2, 0.2}, expired_opts);
    req(expired_sol.status == lp::reference::SolveStatus::resource_limit,
        "MINLP respects common absolute deadline");

    // A linear equality in the source MPS is represented by two valid affine
    // inequalities in the OA NLP/master. Verify both the returned point and
    // the global lower bound; checking feasibility alone could hide a dropped
    // equality in either representation.
    model::Model equality_row_source = source;
    equality_row_source.row_upper[0] = model::Bound::finite(1.5);
    equality_row_source.validate();
    minlp::MinlpProblem equality_row_problem;
    equality_row_problem.source_model = equality_row_source;
    equality_row_problem.nlp = io::make_nlp_model(equality_row_source);
    equality_row_problem.integer_indices = {1};
    const auto equality_row_sol = minlp::solve_minlp(
        equality_row_problem, {0.2, 0.2}, opts);
    req(equality_row_sol.status == lp::reference::SolveStatus::optimal,
        "convex MINLP with a linear MPS equality certifies optimality");
    req(equality_row_sol.integer_feasible && equality_row_sol.x.size() == 2,
        "linear-equality MINLP returns a feasible incumbent");
    req(std::abs(equality_row_sol.x[0] + equality_row_sol.x[1] - 1.5) <= 1e-6,
        "linear source equality is preserved by the NLP subproblem");
    req(std::abs(equality_row_sol.objective - 1.25) <= 1e-3,
        "linear-equality MINLP reaches the known integer optimum");
    req(std::isfinite(equality_row_sol.best_bound) &&
            equality_row_sol.best_bound <= equality_row_sol.objective + 1e-6 &&
            equality_row_sol.relative_gap <= opts.gap_tolerance,
        "MINLP optimal status is backed by a valid finite global bound and gap");
    api::SolveOptions equality_api_opts;
    equality_api_opts.engine = "outer_approx";
    const auto equality_api_sol = api::solve_model(equality_row_source, equality_api_opts);
    req(equality_api_sol.status == lp::reference::SolveStatus::optimal &&
            equality_api_sol.verified,
        "linear-equality MINLP also passes the independent API verifier");

    model::Model infeasible_equality_source = equality_row_source;
    infeasible_equality_source.row_lower[0] = model::Bound::finite(10.0);
    infeasible_equality_source.row_upper[0] = model::Bound::finite(10.0);
    infeasible_equality_source.validate();
    minlp::MinlpProblem infeasible_equality_problem;
    infeasible_equality_problem.source_model = infeasible_equality_source;
    infeasible_equality_problem.nlp = io::make_nlp_model(infeasible_equality_source);
    infeasible_equality_problem.integer_indices = {1};
    const auto infeasible_equality_sol = minlp::solve_minlp(
        infeasible_equality_problem, {0.2, 0.2}, opts);
    req(infeasible_equality_sol.status == lp::reference::SolveStatus::infeasible,
        "infeasible linear equality is not reported as a feasible or optimal MINLP");

    // QUADOBJ and NLOBJ may coexist in one MPS-derived model. The QUADOBJ
    // adds x0^2, so the correct mixed objective is 2*x0^2 + x1^2 and the
    // integer optimum is (0.5, 1) with value 1.5. This catches translation
    // paths that screen the QUADOBJ Hessian but omit it from the NLP objective.
    model::Model mixed_source = source;
    model::SparseMatrixBuilder qbuilder(2, 2);
    qbuilder.add(0, 0, 2.0); // 0.5 * (2*x0^2) = x0^2
    mixed_source.quadratic_matrix = qbuilder.build();
    mixed_source.has_quadratic_objective = true;
    mixed_source.validate();
    minlp::MinlpProblem mixed_problem;
    mixed_problem.source_model = mixed_source;
    mixed_problem.nlp = io::make_nlp_model(mixed_source);
    mixed_problem.integer_indices = {1};
    const auto mixed_sol = minlp::solve_minlp(mixed_problem, {0.2, 0.2}, opts);
    req(mixed_sol.status == lp::reference::SolveStatus::optimal,
        "mixed QUADOBJ/NLOBJ MINLP solves to optimality");
    req(std::abs(mixed_sol.objective - 1.5) < 1e-3,
        "mixed MINLP objective includes the QUADOBJ contribution");

    model::Model indefinite_mixed_source = mixed_source;
    indefinite_mixed_source.nlobj_terms = {{-1.5, 0, 0, true}};
    minlp::MinlpProblem indefinite_mixed_problem;
    indefinite_mixed_problem.source_model = indefinite_mixed_source;
    indefinite_mixed_problem.nlp = io::make_nlp_model(indefinite_mixed_source);
    indefinite_mixed_problem.integer_indices = {1};
    const auto indefinite_mixed_sol = minlp::solve_minlp(
        indefinite_mixed_problem, {0.2, 0.2}, opts);
    req(indefinite_mixed_sol.status == lp::reference::SolveStatus::non_convex_minlp,
        "MINLP convexity screen combines QUADOBJ and NLOBJ Hessians at correct scale");

    minlp::MinlpOptions limited = opts;
    limited.max_iterations = 2;
    limited.gap_tolerance = 1e-15;
    const auto limited_sol = minlp::solve_minlp(problem, {0.2, 1.0}, limited);
    req(limited_sol.status == lp::reference::SolveStatus::iteration_limit,
        "feasible incumbent without requested gap is not reported optimal");
    req(limited_sol.integer_feasible && !limited_sol.x.empty(),
        "iteration limit preserves feasible incumbent");

    minlp::MinlpProblem equality_problem = problem;
    equality_problem.nlp.n_eq = 1;
    equality_problem.nlp.eq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] + x[1] - 1.0};
    };
    equality_problem.nlp.eq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0, 1.0}};
    };
    // `nlp` is legacy input; source_model is authoritative and has no equality.
    const auto stale_equality_sol = minlp::solve_minlp(equality_problem, {0.2, 0.2}, opts);
    req(stale_equality_sol.status == lp::reference::SolveStatus::optimal,
        "stale callback equality metadata does not override the source model");

    minlp::MinlpProblem source_equality_problem = problem;
    nlp::NlpModel equality_callbacks;
    equality_callbacks.n_vars = 2;
    equality_callbacks.objective = [](const std::vector<double>&) { return 0.0; };
    equality_callbacks.gradient = [](const std::vector<double>&) {
        return std::vector<double>(2, 0.0);
    };
    equality_callbacks.n_eq = 1;
    equality_callbacks.eq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] + x[1] - 1.0};
    };
    equality_callbacks.eq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0, 1.0}};
    };
    source_equality_problem.source_model->nlp_callbacks = equality_callbacks;
    const auto equality_sol =
        minlp::solve_minlp(source_equality_problem, {0.2, 0.2}, opts);
    req(equality_sol.status == lp::reference::SolveStatus::unsupported,
        "MINLP rejects source-model equality callbacks that OA master cannot represent");

    minlp::MinlpProblem callback_problem;
    callback_problem.nlp = problem.nlp;
    callback_problem.integer_indices = {1};
    const auto callback_sol = minlp::solve_minlp(callback_problem, {0.2, 0.2}, opts);
    req(callback_sol.status == lp::reference::SolveStatus::unsupported,
        "MINLP rejects callback-only inputs without a structural convexity certificate");

    minlp::MinlpProblem nonconvex_objective = problem;
    nonconvex_objective.source_model->nlobj_terms[0].coefficient = -1.0;
    const auto nonconvex_obj_sol = minlp::solve_minlp(nonconvex_objective, {0.2, 0.2}, opts);
    req(nonconvex_obj_sol.status == lp::reference::SolveStatus::non_convex_minlp,
        "MINLP rejects structurally non-convex quadratic objectives");

    minlp::MinlpProblem nonconvex_constraint = problem;
    nonconvex_constraint.source_model->nlcon_constraints = {
        {"nonconvex_row", 0.0, {{-1.0, 0, 0, true}}}};
    const auto nonconvex_con_sol = minlp::solve_minlp(nonconvex_constraint, {0.2, 0.2}, opts);
    req(nonconvex_con_sol.status == lp::reference::SolveStatus::non_convex_minlp,
        "MINLP rejects structurally non-convex quadratic inequalities");

    minlp::MinlpProblem mismatched_integer_indices = problem;
    mismatched_integer_indices.integer_indices.clear();
    const auto mismatched_sol =
        minlp::solve_minlp(mismatched_integer_indices, {0.2, 0.2}, opts);
    req(mismatched_sol.status == lp::reference::SolveStatus::invalid_model,
        "MINLP rejects integer indices inconsistent with source model types");

    // A linearization master can be unbounded even when a nonlinear convex
    // constraint bounds the original feasible set. That relaxation status is
    // inconclusive and must not leak out as an original-MINLP unbounded claim.
    model::Model bounded_nonlinear_source;
    bounded_nonlinear_source.name = "bounded_nonlinear_master_unbounded";
    model::SparseMatrixBuilder empty_a(0, 2);
    bounded_nonlinear_source.matrix = empty_a.build();
    bounded_nonlinear_source.objective = {-1.0, 0.0};
    bounded_nonlinear_source.variable_name = {"x", "z"};
    bounded_nonlinear_source.variable_lower = {model::Bound::negative_infinity(),
                                               model::Bound::finite(0.0)};
    bounded_nonlinear_source.variable_upper = {model::Bound::positive_infinity(),
                                               model::Bound::finite(0.0)};
    bounded_nonlinear_source.variable_type = {model::VariableType::continuous,
                                              model::VariableType::integer};
    bounded_nonlinear_source.has_nlobj_section = true;
    bounded_nonlinear_source.nlcon_constraints = {
        {"unit_ball", 1.0, {{1.0, 0, 0, true}}}};
    bounded_nonlinear_source.validate();
    minlp::MinlpProblem bounded_nonlinear_problem;
    bounded_nonlinear_problem.source_model = bounded_nonlinear_source;
    bounded_nonlinear_problem.nlp = io::make_nlp_model(bounded_nonlinear_source);
    bounded_nonlinear_problem.integer_indices = {1};
    minlp::MinlpOptions stopped_sqp = opts;
    stopped_sqp.sqp_options.trust_step_scale = 0.0; // Keep first tangent at x=0.
    const auto bounded_nonlinear_sol = minlp::solve_minlp(
        bounded_nonlinear_problem, {0.0, 0.0}, stopped_sqp);
    req(bounded_nonlinear_sol.status == lp::reference::SolveStatus::iteration_limit,
        "unbounded OA master is reported as inconclusive");
    req(bounded_nonlinear_sol.message.find("master relaxation is unbounded") !=
            std::string::npos,
        "unbounded OA master diagnostic explains that original boundedness is unknown");

    // The internal MINLP objective is sign-normalized to minimization. The API
    // must convert the objective and global bound back to the source sense.
    model::Model maximize_source = source;
    maximize_source.objective_sense = model::ObjectiveSense::maximize;
    maximize_source.nlobj_terms = {{-1.0, 0, 0, true}, {-1.0, 1, 1, true}};
    api::SolveOptions api_opts;
    api_opts.engine = "outer_approx";
    const auto api_sol = api::solve_model(maximize_source, api_opts);
    req(api_sol.status == lp::reference::SolveStatus::optimal,
        "maximize MINLP solves with a global bound");
    req(api_sol.verified, "maximize MINLP passes independent API verification");
    req(std::abs(api_sol.objective + 1.25) < 1e-3,
        "maximize MINLP objective is converted back to the original sign");
    req(std::abs(api_sol.best_bound + 1.25) < 1e-3,
        "maximize MINLP lower bound is converted to an original-sense upper bound");

    maximize_source.objective_offset = 2.0;
    const auto offset_api_sol = api::solve_model(maximize_source, api_opts);
    req(offset_api_sol.status == lp::reference::SolveStatus::optimal &&
            offset_api_sol.verified,
        "maximize MINLP with objective offset remains verified");
    req(std::abs(offset_api_sol.objective - 0.75) < 1e-3 &&
            std::abs(offset_api_sol.best_bound - 0.75) < 1e-3,
        "maximize MINLP converts objective offset and global bound correctly");

    std::cout << "minlp basic tests passed\n";
    return 0;
}
