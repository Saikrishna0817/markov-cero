// NLP-01 contract nlp-local-sqp.md §6.8/§6.9: callback violations fail
// closed with bounded, honest statuses and no crashes; limit exits keep the
// honest status, report x0 projection and a feasibility-tolerated incumbent;
// the engine attaches that incumbent only after its own verifier accepts.
#include "markov_cero/api/solve.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

// Unconstrained convex objective used as the callback-violation host.
nlp::NlpModel host_model() {
    nlp::NlpModel model;
    model.name = "callback_host";
    model.n_vars = 2;
    model.objective = [](const std::vector<double>& x) {
        return (x[0] - 1.0) * (x[0] - 1.0) + (x[1] - 2.0) * (x[1] - 2.0);
    };
    model.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * (x[0] - 1.0), 2.0 * (x[1] - 2.0)};
    };
    return model;
}

std::string solve_status(const nlp::NlpModel& model) {
    const auto sol = nlp::solve_sqp(model, {0.0, 0.0});
    return std::string(lp::reference::to_string(sol.status));
}

} // namespace

int main() {
    // §6.8 (a): non-finite gradient -> bounded NumericalFailure naming it.
    {
        auto model = host_model();
        model.gradient = [](const std::vector<double>&) {
            return std::vector<double>{
                std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN()};
        };
        const auto sol = nlp::solve_sqp(model, {0.0, 0.0});
        req(sol.status == lp::reference::SolveStatus::numerical_failure,
            "non-finite gradient fails closed");
        req(contains(sol.message, "gradient callback"), "message names the gradient");
        req(!std::isnan(sol.x[0]), "the returned point stays finite");
    }

    // §6.8 (b): wrong gradient dimension.
    {
        auto model = host_model();
        model.gradient = [](const std::vector<double>&) {
            return std::vector<double>{1.0, 2.0, 3.0};
        };
        const auto sol = nlp::solve_sqp(model, {0.0, 0.0});
        req(sol.status == lp::reference::SolveStatus::numerical_failure,
            "wrong gradient size fails closed");
        req(contains(sol.message, "gradient callback") && contains(sol.message, "expected 2"),
            "message reports the expected dimension");
    }

    // §6.8 (c): objective throws -> exception text surfaces, no crash.
    {
        auto model = host_model();
        model.objective = [](const std::vector<double>&) -> double {
            throw std::runtime_error("objective boom");
        };
        const auto sol = nlp::solve_sqp(model, {0.0, 0.0});
        req(sol.status == lp::reference::SolveStatus::numerical_failure,
            "throwing objective fails closed");
        req(contains(sol.message, "objective boom"), "message carries the exception text");
        req(std::isnan(sol.objective),
            "an objective that could not be evaluated reports unknown, not zero");
    }

    // §6.8 (d): constraint value callback throws (non-standard path too).
    {
        auto model = host_model();
        model.n_ineq = 1;
        model.ineq_constraints = [](const std::vector<double>&) -> std::vector<double> {
            throw std::runtime_error("constraint boom");
        };
        model.ineq_jacobian = [](const std::vector<double>&) {
            return std::vector<std::vector<double>>{{1.0, 0.0}};
        };
        const auto sol = nlp::solve_sqp(model, {0.0, 0.0});
        req(sol.status == lp::reference::SolveStatus::numerical_failure,
            "throwing constraint callback fails closed");
        req(contains(sol.message, "constraint boom"), "constraint exception text surfaces");
    }

    // §6.8 (e): wrong constraint value dimension.
    {
        auto model = host_model();
        model.n_ineq = 1;
        model.ineq_constraints = [](const std::vector<double>&) {
            return std::vector<double>{0.0, 0.0};
        };
        model.ineq_jacobian = [](const std::vector<double>&) {
            return std::vector<std::vector<double>>{{1.0, 0.0}};
        };
        const auto sol = nlp::solve_sqp(model, {0.0, 0.0});
        req(sol.status == lp::reference::SolveStatus::numerical_failure,
            "wrong constraint value size fails closed");
        req(contains(sol.message, "inequality callback"),
            "message names the inequality callback");
    }

    // §6.8 (f): wrong Jacobian column count.
    {
        auto model = host_model();
        model.n_ineq = 1;
        model.ineq_constraints = [](const std::vector<double>&) {
            return std::vector<double>{-1.0};
        };
        model.ineq_jacobian = [](const std::vector<double>&) {
            return std::vector<std::vector<double>>{{1.0, 0.0, 0.0}};
        };
        const auto sol = nlp::solve_sqp(model, {0.0, 0.0});
        req(sol.status == lp::reference::SolveStatus::numerical_failure,
            "wrong Jacobian width fails closed");
        req(contains(sol.message, "columns"), "message reports the column mismatch");
    }

    // §6.8 (g): non-finite constraint value.
    {
        auto model = host_model();
        model.n_ineq = 1;
        model.ineq_constraints = [](const std::vector<double>&) {
            return std::vector<double>{std::numeric_limits<double>::infinity()};
        };
        model.ineq_jacobian = [](const std::vector<double>&) {
            return std::vector<std::vector<double>>{{1.0, 0.0}};
        };
        const auto sol = nlp::solve_sqp(model, {0.0, 0.0});
        req(sol.status == lp::reference::SolveStatus::numerical_failure,
            "non-finite constraint value fails closed");
        req(contains(sol.message, "non-finite"), "message reports the non-finite value");
    }

    // §6.8 (h): malformed x0 rejected before any evaluation.
    {
        const auto sol = nlp::solve_sqp(host_model(), {1.0});
        req(sol.status == lp::reference::SolveStatus::numerical_failure,
            "x0 dimension mismatch stays fail-closed");
        req(contains(sol.message, "x0 dimension mismatch"), "x0 dimension message kept");
        const auto nan_x0 = nlp::solve_sqp(
            host_model(), {std::numeric_limits<double>::quiet_NaN(), 0.0});
        req(nan_x0.status == lp::reference::SolveStatus::numerical_failure,
            "non-finite x0 stays fail-closed");
        req(contains(nan_x0.message, "non-finite"), "non-finite x0 message kept");
    }

    // §6.9 (a): iteration limit keeps the honest status, reports the x0
    // projection, and retains the feasibility-tolerated incumbent.
    {
        auto model = host_model();
        model.lower_bounds = {0.0, 0.0};
        model.upper_bounds = {10.0, 10.0};
        nlp::SqpOptions opts;
        opts.max_iterations = 1;
        const auto sol = nlp::solve_sqp(model, {-5.0, 0.5}, opts);
        req(sol.status == lp::reference::SolveStatus::iteration_limit,
            "one iteration ends at the honest iteration limit");
        req(sol.x0_projected, "x0 projection is disclosed");
        req(std::abs(sol.x0_projection_norm - 5.0) < 1e-15,
            "projection norm equals the clamped coordinate movement");
        req(!sol.best_feasible_x.empty(), "feasibility-tolerated incumbent retained");
        req(nlp::verify_nlp_feasibility(model, sol.best_feasible_x, 1e-6).feasible,
            "retained incumbent is inside the feasibility tolerance");
        const double expected_objective = model.eval_objective(sol.best_feasible_x);
        req(std::abs(sol.best_feasible_objective - expected_objective) < 1e-12,
            "incumbent objective matches the callbacks at the incumbent");
        req(sol.callback_evaluations > 0, "callback evaluations reported");
        // The incumbent is not a KKT claim: the independent checker must
        // still reject it as a solution.
        nlp::SqpSolution as_solution = sol;
        as_solution.x = sol.best_feasible_x;
        req(!nlp::verify_nlp_solution(model, as_solution, 1e-6).accepted,
            "incumbent never masquerades as a KKT point");
    }

    // §6.9 (b) engine gate: a failed callback solve publishes its verified
    // feasible incumbent without upgrading the status.
    {
        using markov_cero::model::Bound;
        using markov_cero::model::ObjectiveSense;
        using markov_cero::model::VariableType;
        markov_cero::model::Model em;
        em.name = "NLP_CALLBACK_FAIL";
        em.objective_sense = ObjectiveSense::minimize;
        em.objective = {0.0};
        em.variable_name = {"x0"};
        em.variable_lower = {Bound::finite(-10.0)};
        em.variable_upper = {Bound::finite(10.0)};
        em.variable_type = {VariableType::continuous};
        // Redundant structural row (x0 <= 10) so the model matches the
        // Path A shape used by api_test; inactive at the start point.
        em.row_name = {"row"};
        em.row_lower = {Bound::negative_infinity()};
        em.row_upper = {Bound::finite(10.0)};
        markov_cero::model::SparseMatrixBuilder mb(1, 1);
        mb.add(0, 0, 1.0);
        em.matrix = mb.build();

        nlp::NlpModel cb;
        cb.name = "broken_gradient";
        cb.n_vars = 1;
        cb.objective = [](const std::vector<double>& x) { return x[0] * x[0]; };
        cb.gradient = [](const std::vector<double>&) {
            return std::vector<double>{1.0, 2.0, 3.0};
        };
        em.nlp_callbacks = cb;
        em.validate();

        markov_cero::api::SolveOptions options;
        const auto res = markov_cero::api::solve_model(em, options);
        req(res.status == lp::reference::SolveStatus::numerical_failure,
            "engine keeps the failure status for a broken gradient");
        req(res.original_verified,
            "engine attaches the verified feasible incumbent (x0 midpoint)");
        req(res.primal.size() == 1 && std::abs(res.primal[0]) < 1e-12,
            "attached incumbent is the feasible start point");
        req(res.assurance == "original_primal_checked",
            "attached incumbent labels as original-primal-checked only");
        req(res.certificate_type != "local_kkt",
            "a failed callback solve never claims local_kkt");
        std::cout << "[+] engine attach on callback failure: status="
                  << lp::reference::to_string(res.status)
                  << " assurance=" << res.assurance << "\n";
    }

    std::cout << "nlp callback guard tests passed\n";
    return 0;
}
