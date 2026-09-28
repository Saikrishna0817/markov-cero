// W1 gate: solve a convex QP through the NLP (SQP) path and check the known
// optimum + KKT residual <= 1e-6 (verification gate).
#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/lbfgs.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
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
    nlp::Lbfgs curvature(10);
    curvature.update({1.0, 0.0}, {2.0, 1.0});
    const auto B = curvature.hessian_matrix(2);
    req(std::abs(B[1] - 1.0) < 1e-12 && std::abs(B[2] - 1.0) < 1e-12,
        "L-BFGS Hessian retains off-diagonal curvature");
    req(B[0] > 0.0 && B[0] * B[3] - B[1] * B[2] > 0.0,
        "L-BFGS Hessian remains positive definite");

    // minimize (x0-1)^2 + (x1-2)^2  ->  optimum (1, 2), objective 0.
    nlp::NlpModel model;
    model.name = "convex_qp_as_nlp";
    model.n_vars = 2;
    model.objective = [](const std::vector<double>& x) {
        const double dx = x[0] - 1.0;
        const double dy = x[1] - 2.0;
        return dx * dx + dy * dy;
    };
    model.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * (x[0] - 1.0), 2.0 * (x[1] - 2.0)};
    };
    model.lower_bounds = {0.0, 0.0};
    model.upper_bounds = {5.0, 5.0};
    model.validate();

    nlp::SqpOptions opts;
    const auto sol = nlp::solve_sqp(model, {0.0, 0.0}, opts);
    req(sol.status == lp::reference::SolveStatus::optimal, "convex QP status optimal");
    req(std::abs(sol.x[0] - 1.0) < 1e-4, "x0 converges to 1");
    req(std::abs(sol.x[1] - 2.0) < 1e-4, "x1 converges to 2");
    req(std::abs(sol.objective) < 1e-8, "objective reaches 0");
    req(sol.kkt_residual <= 1e-6, "KKT residual <= 1e-6");

    const auto rep = nlp::verify_nlp_solution(model, sol, 1e-6);
    req(rep.accepted, "independent KKT verification accepts");

    // An intentionally tiny trusted step cannot turn a nonstationary point
    // into an optimum merely because the iterate stopped moving.
    nlp::SqpOptions tiny_step_options;
    tiny_step_options.trust_step_scale = 1e-16;
    tiny_step_options.max_iterations = 3;
    const auto tiny_step = nlp::solve_sqp(model, {4.0, 4.0}, tiny_step_options);
    req(tiny_step.status != lp::reference::SolveStatus::optimal,
        "tiny inexact steps do not bypass the KKT convergence gate");
    req(!nlp::verify_nlp_solution(model, tiny_step, 1e-6).accepted,
        "nonstationary tiny-step result fails independent KKT verification");

    std::cout << "nlp sqp tests passed\n";
    return 0;
}
