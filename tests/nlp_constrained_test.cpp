// W1 gate: equality-constrained NLP via SQP.
//   minimize  x0^2 + x1^2
//   s.t.      x0 + x1 - 2 = 0   ->  optimum (1, 1), objective 2.
#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

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
    nlp::NlpModel model;
    model.name = "equality_constrained";
    model.n_vars = 2;
    model.n_eq = 1;
    model.objective = [](const std::vector<double>& x) {
        return x[0] * x[0] + x[1] * x[1];
    };
    model.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * x[0], 2.0 * x[1]};
    };
    model.eq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] + x[1] - 2.0};
    };
    model.eq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0, 1.0}};
    };
    model.validate();

    nlp::SqpOptions opts;
    const auto sol = nlp::solve_sqp(model, {3.0, 0.0}, opts);
    req(sol.status == lp::reference::SolveStatus::optimal, "equality-constrained status");
    req(std::abs(sol.x[0] - 1.0) < 1e-4, "x0 converges to 1");
    req(std::abs(sol.x[1] - 1.0) < 1e-4, "x1 converges to 1");
    req(std::abs(sol.objective - 2.0) < 1e-6, "objective reaches 2");
    req(std::abs(sol.constraint_violation) < 1e-6, "equality satisfied");

    const auto rep = nlp::verify_nlp_solution(model, sol, 1e-6);
    req(rep.accepted, "independent KKT verification accepts");
    req(nlp::verify_nlp_feasibility(model, sol.x, 1e-6).feasible,
        "independent primal-only feasibility check accepts the solution");
    req(!nlp::verify_nlp_feasibility(model, {0.0, 0.0}, 1e-6).feasible,
        "independent primal-only feasibility check rejects violated equality");
    req(!nlp::verify_nlp_feasibility(model, {1.0}, 1e-6).feasible,
        "independent primal-only feasibility check rejects wrong dimension");

    nlp::NlpModel inactive_inequality;
    inactive_inequality.n_vars = 1;
    inactive_inequality.n_ineq = 1;
    inactive_inequality.objective = [](const std::vector<double>&) { return 0.0; };
    inactive_inequality.gradient = [](const std::vector<double>&) {
        return std::vector<double>{-1.0};
    };
    inactive_inequality.ineq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] - 1.0};
    };
    inactive_inequality.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0}};
    };
    nlp::SqpSolution invalid_kkt;
    invalid_kkt.x = {0.0};
    invalid_kkt.ineq_multipliers = {1.0};
    const auto bad_kkt = nlp::verify_nlp_solution(inactive_inequality, invalid_kkt, 1e-6);
    req(!bad_kkt.accepted, "independent KKT verifier rejects inactive positive multiplier");
    req(std::abs(bad_kkt.stationarity_residual) < 1e-12,
        "adversarial multiplier satisfies stationarity");
    req(std::abs(bad_kkt.complementarity_residual - 1.0) < 1e-12,
        "independent KKT verifier reports complementarity violation");
    nlp::SqpSolution nan_primal = invalid_kkt;
    nan_primal.x = {std::numeric_limits<double>::quiet_NaN()};
    req(!nlp::verify_nlp_solution(inactive_inequality, nan_primal, 1e-6).accepted,
        "independent KKT verifier rejects non-finite primal values");

    inactive_inequality.gradient = [](const std::vector<double>&) {
        return std::vector<double>{std::numeric_limits<double>::quiet_NaN()};
    };
    invalid_kkt.ineq_multipliers = {0.0};
    req(!nlp::verify_nlp_solution(inactive_inequality, invalid_kkt, 1e-6).accepted,
        "independent KKT verifier rejects non-finite gradient values");

    inactive_inequality.gradient = [](const std::vector<double>&) {
        return std::vector<double>{0.0};
    };
    inactive_inequality.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{std::numeric_limits<double>::quiet_NaN()}};
    };
    req(!nlp::verify_nlp_solution(inactive_inequality, invalid_kkt, 1e-6).accepted,
        "independent KKT verifier rejects non-finite Jacobian values");
    inactive_inequality.ineq_jacobian = [](const std::vector<double>&) {
        throw std::runtime_error("adversarial Jacobian failure");
        return std::vector<std::vector<double>>{};
    };
    req(!nlp::verify_nlp_solution(inactive_inequality, invalid_kkt, 1e-6).accepted,
        "independent KKT verifier rejects callback exceptions");
    inactive_inequality.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0}};
    };
    invalid_kkt.ineq_multipliers.clear();
    req(!nlp::verify_nlp_solution(inactive_inequality, invalid_kkt, 1e-6).accepted,
        "independent KKT verifier rejects wrong multiplier dimensions");

    // The accepted line-search step crosses from an inactive to active
    // inequality. Direct SQP status and the independent verifier must agree
    // at that accepted point (no stale pre-step constraint values).
    nlp::NlpModel changing_active_set;
    changing_active_set.n_vars = 1;
    changing_active_set.n_ineq = 1;
    changing_active_set.objective = [](const std::vector<double>& x) {
        return (x[0] - 2.0) * (x[0] - 2.0);
    };
    changing_active_set.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * (x[0] - 2.0)};
    };
    changing_active_set.ineq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] - 1.5};
    };
    changing_active_set.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0}};
    };
    const auto active_sol = nlp::solve_sqp(changing_active_set, {0.0});
    req(active_sol.status == lp::reference::SolveStatus::optimal,
        "SQP converges after activating an inequality");
    req(std::abs(active_sol.x[0] - 1.5) < 1e-5,
        "accepted SQP point lies on the activated inequality");
    req(nlp::verify_nlp_solution(changing_active_set, active_sol, 1e-6).accepted,
        "direct SQP result passes independent KKT verification at accepted point");

    std::cout << "nlp constrained tests passed\n";
    return 0;
}
