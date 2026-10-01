// NLP-02 contract nlp-restoration.md §7: elastic restoration — recovery of
// a feasible-but-linearization-infeasible model, the disabled old path,
// honest bounded failure on a truly infeasible model, and a healthy-path
// regression (no restoration counters when the standard QP path runs).
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

// Frozen failure corpus case 1 (contract §2.1): minimized feasible failure.
// h(x) = x^2 - 1 = 0 is feasible (x = ±1) but at x0 = 0 the linearized
// equality reads 0·d = 1, so the standard SQP QP is primal-infeasible.
nlp::NlpModel make_x2_minus_one() {
    nlp::NlpModel model;
    model.name = "x2_minus_one";
    model.n_vars = 1;
    model.n_eq = 1;
    model.objective = [](const std::vector<double>& x) {
        const double a = x[0] - 1.0;
        return a * a;
    };
    model.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * (x[0] - 1.0)};
    };
    model.eq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] * x[0] - 1.0};
    };
    model.eq_jacobian = [](const std::vector<double>& x) {
        return std::vector<std::vector<double>>{{2.0 * x[0]}};
    };
    model.validate();
    return model;
}

// Frozen failure corpus case 2 (contract §2.2): truly infeasible linear
// pair — original violation cannot decrease anywhere.
nlp::NlpModel make_linear_pair() {
    nlp::NlpModel model;
    model.name = "linear_pair";
    model.n_vars = 1;
    model.n_ineq = 2;
    model.objective = [](const std::vector<double>& x) { return x[0] * x[0]; };
    model.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * x[0]};
    };
    model.ineq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0], 1.0 - x[0]};
    };
    model.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0}, {-1.0}};
    };
    model.validate();
    return model;
}

nlp::NlpModel make_equality_quadratic() {
    nlp::NlpModel model;
    model.name = "equality_quadratic";
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
    return model;
}

} // namespace

int main() {
    // §7.1 recovery: the minimized feasible failure converges through
    // elastic restoration; the published point passes the independent
    // verifier (nlp-local-sqp §5 gate unchanged).
    {
        const auto model = make_x2_minus_one();
        const auto sol = nlp::solve_sqp(model, {0.0});
        req(sol.status == lp::reference::SolveStatus::optimal,
            "x2_minus_one recovers to the solver KKT status");
        req(sol.restoration_steps >= 1, "recovery used at least one restoration step");
        req(sol.restoration_failures == 0, "recovery needed no rejected attempts");
        req(!sol.restoration_exhausted, "recovery did not exhaust the budget");
        req(sol.constraint_violation < 1e-6, "recovered point satisfies x^2 = 1");
        req(std::abs(sol.x[0] - 1.0) < 1e-6, "recovered point is x = 1");
        const auto report = nlp::verify_nlp_solution(model, sol, 1e-6);
        req(report.accepted, "recovered point passes the independent KKT checker");
        req(sol.message.find("elastic restoration steps=") != std::string::npos,
            "exit message discloses the restoration step count");
        std::cout << "[+] x2_minus_one recovered: steps=" << sol.restoration_steps
                  << " message=" << sol.message << "\n";
    }

    // §7.2 old path: elastic_restoration = false reproduces the NLP-01
    // immediate inconclusive failure with zero restoration counters.
    {
        const auto model = make_x2_minus_one();
        nlp::SqpOptions opts;
        opts.elastic_restoration = false;
        const auto sol = nlp::solve_sqp(model, {0.0}, opts);
        req(sol.status == lp::reference::SolveStatus::numerical_failure,
            "disabled restoration keeps the immediate inconclusive failure");
        req(sol.message.find("primal-infeasible") != std::string::npos,
            "disabled path names the linearized infeasibility");
        req(sol.message.find("elastic restoration is disabled") != std::string::npos,
            "disabled path names the off switch");
        req(sol.restoration_steps == 0 && sol.restoration_failures == 0,
            "disabled path reports zero restoration counters");
        req(!sol.restoration_exhausted, "disabled path does not set exhausted");
        std::cout << "[+] disabled path: " << sol.message << "\n";
    }

    // §7.3 truly infeasible: bounded rejected attempts end inconclusively;
    // never Infeasible, never Optimal, never a verified feasible incumbent.
    {
        const auto model = make_linear_pair();
        const auto sol = nlp::solve_sqp(model, {0.5});
        const bool honest = sol.status == lp::reference::SolveStatus::numerical_failure ||
                            sol.status == lp::reference::SolveStatus::iteration_limit ||
                            sol.status == lp::reference::SolveStatus::resource_limit;
        req(honest, "linear pair stops with an inconclusive honest status");
        req(sol.status != lp::reference::SolveStatus::infeasible,
            "restoration never proves infeasibility");
        req(sol.status != lp::reference::SolveStatus::optimal,
            "linear pair never reports the solver KKT status");
        req(sol.restoration_exhausted, "linear pair exhausts the restoration budget");
        req(sol.restoration_failures > 5, "exhaustion follows the bounded consecutive failures");
        req(sol.restoration_steps == 0, "linear pair accepts no restoration step");
        req(sol.best_feasible_x.empty(),
            "no iterate of the linear pair is inside the feasibility tolerance");
        req(sol.message.find("elastic restoration") != std::string::npos,
            "exhaustion message names elastic restoration");
        std::cout << "[+] linear pair exhausted: " << sol.message << "\n";
    }

    // §7.4 healthy-path regression: the standard QP path runs untouched —
    // no restoration counters anywhere.
    {
        const auto model = make_equality_quadratic();
        const auto sol = nlp::solve_sqp(model, {3.0, 0.0});
        req(sol.status == lp::reference::SolveStatus::optimal,
            "equality quadratic still converges");
        req(sol.restoration_steps == 0 && sol.restoration_failures == 0,
            "healthy path never touches restoration");
        req(!sol.restoration_exhausted, "healthy path never sets exhausted");
        req(sol.message.find("elastic restoration") == std::string::npos,
            "healthy message carries no restoration suffix");
        std::cout << "[+] healthy path regression ok\n";
    }

    std::cout << "nlp restoration tests passed\n";
    return 0;
}
