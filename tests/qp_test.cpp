#include "qp_test_fixtures.hpp"
int main() {
    using namespace markov_cero;
    using namespace markov_cero::qp;

    std::cout << "[Test] 1. KktSolver LDL^T factorization and solve\n";
    qp_scenario_0();

    std::cout << "[Test] 2. Unconstrained Convex QP\n";
    // min (1/2)(x1^2 + x2^2) - x1 - 2*x2
    // Analytic minimum: x1* = 1, x2* = 2, f(x*) = 0.5*(1+4) - 1 - 4 = -2.5
    qp_scenario_1();

    std::cout << "[Test] 3. Constrained Convex QP (Equality & Inequality)\n";
    // min (1/2)(4*x1^2 + 2*x1*x2 + 2*x2^2) + x1 + 2*x2
    // s.t. x1 + x2 >= 1, x1 >= 0, x2 >= 0
    qp_scenario_2();

    std::cout << "[Test] 4. Non-Convex QP Rejection\n";
    // P has negative eigenvalue -> non-convex
    qp_scenario_3();

    std::cout << "[Test] 4a. Sparse convexity classification and large dimensions\n";
    qp_scenario_4();

    std::cout << "[Test] 5. Primal Infeasible QP\n";
    // x1 + x2 >= 5 and x1 + x2 <= 2
    qp_scenario_5();

    std::cout << "[Test] 6. Maros-Meszaros Style Benchmark Form (HUEBNER style)\n";
    // min (1/2) sum_i (x_i - i)^2 s.t. sum_i x_i = N*(N+1)/2, x >= 0
    qp_scenario_6();

    std::cout << "[Test] 7. Mixed-Integer Quadratic Program (MIQP)\n";
    // min (1/2)(2*x1^2 + 2*x2^2) - 2.4*x1 - 5.6*x2 + 9.28
    // s.t. x1 + x2 <= 3, x1, x2 in {0, 1, 2, 3} integers
    // Integer optimum: x* = (1, 2), f(x*) = 0.68
    qp_scenario_7();

    std::cout << "[Test] 8. Adaptive Rho Update and Refactorization Counter\n";
    qp_scenario_8();

    std::cout << "[Pass] All QP tests passed successfully!\n";
    return 0;
}
