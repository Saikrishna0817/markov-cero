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

    std::cout << "[Test] 9. MIQP node QP bound overlay\n";
    qp_scenario_9();

    std::cout << "[Test] 10. Repeated QP factor cache and verification\n";
    QuadraticModel repeated;
    repeated.P.dimension = 2;
    repeated.P.column_offsets = {0, 1, 2};
    repeated.P.row_indices = {0, 1};
    repeated.P.values = {2, 2};
    repeated.q = {-2, -2};
    repeated.A.rows = 2;
    repeated.A.columns = 2;
    repeated.A.column_offsets = {0, 1, 2};
    repeated.A.row_indices = {0, 1};
    repeated.A.values = {1, 1};
    repeated.l = {0.2, 0.3};
    repeated.u = {2, 2};
    repeated.validate();
    QpOptions repeat_options;
    repeat_options.adaptive_rho = false;
    repeat_options.absolute_tolerance = 1e-5;
    repeat_options.relative_tolerance = 1e-5;
    AdmmQpSolver session;
    for (int step = 0; step < 8; ++step) {
        repeated.l[0] = 0.2 + 0.02 * step;
        repeated.u[1] = 2.0 - 0.02 * step;
        const auto cold = solve_qp(repeated, repeat_options);
        const auto hot = solve_qp(repeated, repeat_options, session, false);
        require(cold.status == QpStatus::optimal && hot.status == cold.status,
                "QP repeat status parity");
        require(cold.verified && hot.verified, "each QP repeat is verified");
        require(verify_qp_solution(repeated, cold).passed &&
                    verify_qp_solution(repeated, hot).passed,
                "QP repeated witnesses independently verified");
        require(std::abs(cold.objective_value - hot.objective_value) < 1e-9,
                "QP repeat objective parity");
        require(cold.iterations == hot.iterations, "QP repeat iteration parity");
        require(hot.kkt_symbolic_reuse == (step > 0 ? 1U : 0U),
                "QP symbolic cache hit cadence");
    }
    require(session.symbolic_factorizations() == 1 && session.symbolic_reuses() == 7,
            "QP session symbolic counters");
    KktSolver kkt;
    require(kkt.factorize(repeated.P, repeated.A, 1e-6, {0.1, 0.1}),
            "KKT cache seed");
    const auto seed_fingerprint = kkt.cached_pattern_fingerprint();
    require(kkt.factorize(repeated.P, repeated.A, 1e-6, {0.2, 0.2}) &&
                kkt.symbolic_reuses() == 1 &&
                kkt.cached_pattern_fingerprint() == seed_fingerprint,
            "rho numeric change reuses symbolic pattern");
    auto changed = repeated.A;
    changed.row_indices = {1, 0};
    require(kkt.factorize(repeated.P, changed, 1e-6, {0.2, 0.2}) &&
                kkt.symbolic_factorizations() == 2 &&
                kkt.last_pattern_fingerprint() != seed_fingerprint,
            "changed A pattern invalidates symbolic cache");
    kkt.enable_symbolic_cache(false);
    require(kkt.factorize(repeated.P, changed, 1e-6, {0.2, 0.2}) &&
                kkt.symbolic_factorizations() == 3 && kkt.symbolic_reuses() == 1,
            "disabled cache repeats the full symbolic path");
    kkt.enable_symbolic_cache(true);
    SparseSymmetricMatrix smaller_p;
    smaller_p.dimension = 1;
    smaller_p.column_offsets = {0, 1};
    smaller_p.row_indices = {0};
    smaller_p.values = {2};
    linalg::SparseCsc taller_a;
    taller_a.rows = 3;
    taller_a.columns = 1;
    taller_a.column_offsets = {0, 3};
    taller_a.row_indices = {0, 1, 2};
    taller_a.values = {1, 1, 1};
    require(kkt.factorize(smaller_p, taller_a, 1e-6, {0.2, 0.2, 0.2}) &&
                kkt.symbolic_factorizations() == 4 && kkt.symbolic_reuses() == 1,
            "same total dimension but changed KKT shape misses cache");

    std::cout << "[Pass] All QP tests passed successfully!\n";
    return 0;
}
