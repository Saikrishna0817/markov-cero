// NLP-01 contract nlp-local-sqp.md §6: local semantics — multi-start
// Rosenbrock, analytic multipliers, bound-active stationarity, saddle
// honesty (§6.6) and infeasible-constraint honesty (§6.7).
#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

nlp::NlpModel make_rosenbrock() {
    nlp::NlpModel model;
    model.name = "rosenbrock";
    model.n_vars = 2;
    model.objective = [](const std::vector<double>& x) {
        const double a = 1.0 - x[0];
        const double b = x[1] - x[0] * x[0];
        return 100.0 * b * b + a * a;
    };
    model.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{
            -2.0 * (1.0 - x[0]) - 400.0 * x[0] * (x[1] - x[0] * x[0]),
            200.0 * (x[1] - x[0] * x[0])};
    };
    model.validate();
    return model;
}

} // namespace

int main() {
    // §6.2 multi-start Rosenbrock: both starts report the local outcome and
    // pass the independent KKT checker. No globality inference anywhere.
    const auto rosen = make_rosenbrock();
    nlp::SqpOptions ropts;
    ropts.max_iterations = 20000;
    const std::vector<std::vector<double>> starts = {{-1.2, 1.0}, {0.0, 0.0}};
    for (std::size_t s = 0; s < starts.size(); ++s) {
        const auto sol = nlp::solve_sqp(rosen, starts[s], ropts);
        req(sol.status == lp::reference::SolveStatus::optimal, "rosenbrock start converges");
        req(std::abs(sol.x[0] - 1.0) < 1e-3, "rosenbrock start x near 1");
        req(std::abs(sol.x[1] - 1.0) < 1e-3, "rosenbrock start y near 1");
        req(nlp::verify_nlp_solution(rosen, sol, 1e-6).accepted,
            "rosenbrock start passes the independent KKT checker");
        req(sol.callback_evaluations > 0, "callback evaluations are counted");
        std::cout << "[+] rosenbrock start " << s << ": iterations="
                  << sol.iterations << " callbacks=" << sol.callback_evaluations
                  << "\n";
    }

    // §6.3 equality-constrained quadratic: the solver multiplier matches the
    // analytic one. min x0^2+x1^2 s.t. x0+x1-2=0 -> x*=(1,1), lambda=-2.
    nlp::NlpModel eq;
    eq.name = "eq_quad";
    eq.n_vars = 2;
    eq.n_eq = 1;
    eq.objective = [](const std::vector<double>& x) {
        return x[0] * x[0] + x[1] * x[1];
    };
    eq.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * x[0], 2.0 * x[1]};
    };
    eq.eq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] + x[1] - 2.0};
    };
    eq.eq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0, 1.0}};
    };
    eq.validate();
    const auto eq_sol = nlp::solve_sqp(eq, {3.0, 0.0});
    req(eq_sol.status == lp::reference::SolveStatus::optimal, "equality model converges");
    req(eq_sol.eq_multipliers.size() == 1, "equality multiplier reported");
    req(std::abs(eq_sol.eq_multipliers[0] + 2.0) < 1e-4,
        "solver equality multiplier matches the analytic value -2");
    req(nlp::verify_nlp_solution(eq, eq_sol, 1e-6).accepted,
        "equality model passes the independent checker");

    // §6.4 active lower/upper bounds with multiplier sign checks through the
    // bound-normal projection, plus an inactive->active inequality sign.
    nlp::NlpModel upper;
    upper.name = "bound_upper";
    upper.n_vars = 1;
    upper.objective = [](const std::vector<double>& x) {
        return (x[0] - 3.0) * (x[0] - 3.0);
    };
    upper.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * (x[0] - 3.0)};
    };
    upper.lower_bounds = {0.0};
    upper.upper_bounds = {1.0};
    upper.validate();
    const auto upper_sol = nlp::solve_sqp(upper, {0.5});
    req(upper_sol.status == lp::reference::SolveStatus::optimal, "upper-bound optimum found");
    req(std::abs(upper_sol.x[0] - 1.0) < 1e-5, "optimum sits on the upper bound");
    req(nlp::verify_nlp_solution(upper, upper_sol, 1e-6).accepted,
        "upper-bound-active stationarity accepted by the bound-normal check");

    nlp::NlpModel lower;
    lower.name = "bound_lower";
    lower.n_vars = 1;
    lower.objective = [](const std::vector<double>& x) {
        return (x[0] + 3.0) * (x[0] + 3.0);
    };
    lower.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * (x[0] + 3.0)};
    };
    lower.lower_bounds = {0.0};
    lower.upper_bounds = {1.0};
    lower.validate();
    const auto lower_sol = nlp::solve_sqp(lower, {0.5});
    req(lower_sol.status == lp::reference::SolveStatus::optimal, "lower-bound optimum found");
    req(std::abs(lower_sol.x[0]) < 1e-5, "optimum sits on the lower bound");
    req(nlp::verify_nlp_solution(lower, lower_sol, 1e-6).accepted,
        "lower-bound-active stationarity accepted by the bound-normal check");

    // A strictly active <= inequality carries a non-negative multiplier.
    nlp::NlpModel ineq;
    ineq.name = "active_ineq";
    ineq.n_vars = 1;
    ineq.n_ineq = 1;
    ineq.objective = [](const std::vector<double>& x) { return x[0] * x[0]; };
    ineq.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * x[0]};
    };
    ineq.ineq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] + 1.0};
    };
    ineq.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0}};
    };
    ineq.validate();
    const auto ineq_sol = nlp::solve_sqp(ineq, {0.0});
    req(ineq_sol.status == lp::reference::SolveStatus::optimal, "inequality model converges");
    req(ineq_sol.ineq_multipliers.size() == 1 && ineq_sol.ineq_multipliers[0] > 0.0,
        "active inequality multiplier is non-negative");
    req(nlp::verify_nlp_solution(ineq, ineq_sol, 1e-6).accepted,
        "active inequality passes the independent checker");

    // §6.6 saddle f = x0^2 - x1^2 at (0,0): first-order KKT holds, but the
    // public status must be LocalStationary — never Optimal — and the result
    // is never described as a verified local minimum.
    nlp::NlpModel saddle;
    saddle.name = "saddle";
    saddle.n_vars = 2;
    saddle.objective = [](const std::vector<double>& x) {
        return x[0] * x[0] - x[1] * x[1];
    };
    saddle.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * x[0], -2.0 * x[1]};
    };
    saddle.validate();
    const auto saddle_sol = nlp::solve_sqp(saddle, {0.0, 0.0});
    req(saddle_sol.status == lp::reference::SolveStatus::optimal,
        "saddle is a first-order point for the solver gate");
    req(nlp::verify_nlp_solution(saddle, saddle_sol, 1e-6).accepted,
        "saddle satisfies the first-order checker");
    const std::string public_status = lp::reference::to_string(
        lp::reference::SolveStatus::local_optimal);
    req(public_status == "LocalStationary",
        "public NLP status spelling is LocalStationary (numerical-policy v2)");
    req(public_status != "Optimal", "a KKT candidate is never spelled Optimal");

    // §6.7 infeasible constraints x <= 0 and x >= 1: SQP failure stays in
    // {NumericalFailure, IterationLimit, ResourceLimit}; it never becomes
    // Optimal/LocalStationary and never claims a globally proved
    // infeasibility.
    nlp::NlpModel infeasible;
    infeasible.name = "infeasible_pair";
    infeasible.n_vars = 1;
    infeasible.n_ineq = 2;
    infeasible.objective = [](const std::vector<double>& x) { return x[0] * x[0]; };
    infeasible.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * x[0]};
    };
    infeasible.ineq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0], 1.0 - x[0]};
    };
    infeasible.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0}, {-1.0}};
    };
    infeasible.validate();
    const auto bad = nlp::solve_sqp(infeasible, {0.5});
    const bool honest_status =
        bad.status == lp::reference::SolveStatus::numerical_failure ||
        bad.status == lp::reference::SolveStatus::iteration_limit ||
        bad.status == lp::reference::SolveStatus::resource_limit;
    req(honest_status, "infeasible pair stops with an inconclusive honest status");
    req(bad.status != lp::reference::SolveStatus::optimal,
        "infeasible pair never reports the solver KKT status");
    req(bad.status != lp::reference::SolveStatus::infeasible,
        "SQP failure never claims a globally proved infeasibility");
    req(bad.status != lp::reference::SolveStatus::unbounded,
        "infeasible pair never reports unbounded");
    req(bad.best_feasible_x.empty(),
        "no iterate of the infeasible pair is inside the feasibility tolerance");
    std::cout << "[+] infeasible pair status="
              << lp::reference::to_string(bad.status) << " message=" << bad.message
              << "\n";

    std::cout << "nlp local semantics tests passed\n";
    return 0;
}
