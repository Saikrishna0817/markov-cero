// W1 gate: Rosenbrock via SQP. Non-convex, but the known minimum (1,1) must
// be reached within 1e-3 from the standard start (-1.2, 1) — the plan asks for
// convergence, not global optimality.
#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"

#include <cmath>
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

    nlp::SqpOptions opts;
    // Rosenbrock's narrow valley needs more major iterations than a convex QP.
    opts.max_iterations = 20000;
    opts.lbfgs_memory = 10;
    const auto sol = nlp::solve_sqp(model, {-1.2, 1.0}, opts);
    req(sol.status == lp::reference::SolveStatus::optimal, "rosenbrock converges");
    req(std::abs(sol.x[0] - 1.0) < 1e-3, "rosenbrock x within 1e-3 of (1,1)");
    req(std::abs(sol.x[1] - 1.0) < 1e-3, "rosenbrock y within 1e-3 of (1,1)");

    std::cout << "nlp rosenbrock tests passed\n";
    return 0;
}
