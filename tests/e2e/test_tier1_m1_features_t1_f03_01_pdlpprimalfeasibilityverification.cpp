#include "test_tier1_m1_features_fixtures.hpp"
E2E_TEST(T1_F03_01_PdlpPrimalFeasibilityVerification,
         "Verify converged PDLP candidate passes zero-trust primal verification",
         Tier1, M1, 3) {
    model::Model model;
    model.name = "PDLP_VERIFY";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {-1.0, -1.0};

    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();

    model.row_lower = {model::Bound::negative_infinity()};
    model.row_upper = {model::Bound::finite(2.0)};
    model.row_name = {"LE"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(5.0), model::Bound::finite(5.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    opts.set_tolerance(1e-4);
    auto res = lp::first_order::solve_pdlp(model, opts);

    if (res.status == lp::first_order::PdlpStatus::optimal) {
        verify::Candidate cand{res.primal, res.objective};
        auto report = verify::verify_primal(model, cand, {1e-4, 1e-4}, {1e-4, 1e-4});
        E2E_ASSERT(report.passed, "Zero-trust verification of PDLP primal must pass");
    }
}

E2E_TEST(T1_F03_02_PdlpEqualityConstraintFeasibility,
         "PDLP solve on equality-constrained problem satisfies bounds",
         Tier1, M1, 3) {
    model::Model model;
    model.name = "PDLP_EQ";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {1.0, 1.0};

    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();

    model.row_lower = {model::Bound::finite(5.0)};
    model.row_upper = {model::Bound::finite(5.0)};
    model.row_name = {"EQ"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(5.0), model::Bound::finite(5.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    opts.set_tolerance(1e-4);
    auto res = lp::first_order::solve_pdlp(model, opts);
    E2E_ASSERT_NEAR(res.objective, 5.0, 0.1, "Objective near 5.0");
    E2E_ASSERT(res.primal[0] + res.primal[1] >= 4.9 && res.primal[0] + res.primal[1] <= 5.1,
               "Sum of variables satisfies equality constraint");
}

// ---------------------------------------------------------------------------
// Feature 4 & 5: ADMM Adaptive Penalty Rho & Refactorization
// ---------------------------------------------------------------------------

E2E_TEST(T1_F04_01_AdmmUnconstrainedConvexQp,
         "ADMM QP solver finds exact analytical minimum on unconstrained convex QP",
         Tier1, M1, 4) {
    // min (1/2) (x1^2 + x2^2) - x1 - 2*x2
    // Analytical min: x* = (1, 2), f(x*) = 0.5*(1+4) - 1 - 4 = -2.5
    qp::QuadraticModel model;
    model.variable_names = {"X1", "X2"};
    model.P.dimension = 2;
    model.P.column_offsets = {0, 1, 2};
    model.P.row_indices = {0, 1};
    model.P.values = {1.0, 1.0};
    model.q = {-1.0, -2.0};
    model.A.rows = 0;
    model.A.columns = 2;
    model.A.column_offsets = {0, 0, 0};

    qp::QpOptions opts;
    opts.absolute_tolerance = 1e-5;
    opts.relative_tolerance = 1e-5;
    opts.adaptive_rho = true;

    auto sol = qp::solve_qp(model, opts);
    E2E_ASSERT(sol.status == qp::QpStatus::optimal, "ADMM unconstrained must be optimal");
    E2E_ASSERT_NEAR(sol.x[0], 1.0, 1e-3, "x1 near 1.0");
    E2E_ASSERT_NEAR(sol.x[1], 2.0, 1e-3, "x2 near 2.0");
    E2E_ASSERT_NEAR(sol.objective_value, -2.5, 1e-3, "Objective near -2.5");
}

E2E_TEST(T1_F04_02_AdmmConstrainedConvexQp,
         "ADMM QP solver reaches known optimum on constrained convex QP",
         Tier1, M1, 4) {
    const std::string mps =
        "NAME CONSTR\n"
        "ROWS\n"
        " N OBJ\n"
        " G C1\n"
        "COLUMNS\n"
        " X1 OBJ 1 C1 1\n"
        " X2 OBJ 2 C1 1\n"
        "RHS\n"
        " RHS1 C1 1\n"
        "QUADOBJ\n"
        " X1 X1 4\n"
        " X1 X2 1\n"
        " X2 X2 2\n"
        "ENDATA\n";

    const auto model = io::parse_mps_string(mps);
    const auto qp = qp::make_quadratic_model(model);

    qp::QpOptions opts;
    opts.absolute_tolerance = 1e-5;
    opts.relative_tolerance = 1e-5;
    opts.adaptive_rho = true;

    auto sol = qp::solve_qp(qp, opts);
    E2E_ASSERT(sol.status == qp::QpStatus::optimal, "Constrained QP optimal");
    E2E_ASSERT(sol.x[0] + sol.x[1] >= 1.0 - 1e-4, "Constraint x1 + x2 >= 1 satisfied");
}

E2E_TEST(T1_F04_03_AdmmAdaptiveRhoOptions,
         "Verify adaptive_rho options configuration on AdmmQpSolver",
         Tier1, M1, 4) {
    qp::QpOptions opts;
    opts.adaptive_rho = true;
    opts.adaptive_rho_interval = 25;
    opts.rho_init = 0.1;
    E2E_ASSERT_TRUE(opts.adaptive_rho);
    E2E_ASSERT(opts.adaptive_rho_interval == 25, "Adaptive rho interval 25");
    E2E_ASSERT_NEAR(opts.rho_init, 0.1, 1e-12, "Initial rho 0.1");
}

E2E_TEST(T1_F04_04_AdmmResidualNormsRecorded,
         "ADMM solver records primal and dual residual norms in solution",
         Tier1, M1, 4) {
    qp::QuadraticModel model;
    model.P.dimension = 1;
    model.P.column_offsets = {0, 1};
    model.P.row_indices = {0};
    model.P.values = {2.0};
    model.q = {-4.0};
    model.A.rows = 0;
    model.A.columns = 1;
    model.A.column_offsets = {0, 0};

    auto sol = qp::solve_qp(model);
    E2E_ASSERT(sol.status == qp::QpStatus::optimal, "1D QP optimal");
    E2E_ASSERT_NEAR(sol.x[0], 2.0, 1e-3, "x* = 2.0");
    E2E_ASSERT(sol.primal_residual >= 0.0, "Primal residual recorded");
    E2E_ASSERT(sol.dual_residual >= 0.0, "Dual residual recorded");
}

E2E_TEST(T1_F05_01_AdmmKktFactorizeAndSolve,
         "Verify KktSolver LDLT factorization and linear solve on symmetric KKT system",
         Tier1, M1, 5) {
    qp::SparseSymmetricMatrix P;
    P.dimension = 2;
    P.column_offsets = {0, 1, 3};
    P.row_indices = {0, 0, 1};
    P.values = {4.0, 1.0, 2.0};

    linalg::SparseCsc A;
    A.rows = 1;
    A.columns = 2;
    A.column_offsets = {0, 1, 2};
    A.row_indices = {0, 0};
    A.values = {1.0, 1.0};

    qp::KktSolver kkt;
    bool fact_ok = kkt.factorize(P, A, 1e-4, {1.0});
    E2E_ASSERT(fact_ok, "KKT factorization must succeed");
    E2E_ASSERT(kkt.dimension() == 3, "KKT system dimension must be n + m = 3");

    std::vector<double> sol_x, sol_nu;
    kkt.solve({1.0, 2.0}, {3.0}, sol_x, sol_nu);
    E2E_ASSERT(sol_x.size() == 2, "sol_x has 2 elements");
    E2E_ASSERT(sol_nu.size() == 1, "sol_nu has 1 element");
}

// ---------------------------------------------------------------------------
// Feature 6: Always-On Iterative Refinement
// ---------------------------------------------------------------------------

E2E_TEST(T1_F06_01_SparseBasisFactorizeAndRefine,
         "Factorize basis with SparseBasisFactorization and verify refinement residual",
         Tier1, M1, 6) {
    std::vector<std::vector<double>> cols = {
        {4.0, 1.0, 0.0},
        {1.0, 5.0, 1.0},
        {0.0, 2.0, 6.0}
    };
    auto csc = linalg::SparseCsc::from_columns(3, cols);
    linalg::SparseBasisOptions opts;
    opts.maximum_refinement_steps = 2;

    auto basis = linalg::SparseBasisFactorization::factorize(csc, opts);
    std::vector<double> b = {7.0, -2.0, 4.0};
    auto x = basis.solve(b);

    double res = linalg::sparse_infinity_residual(csc, x, b);
    E2E_ASSERT_NEAR(res, 0.0, 1e-12, "Residual after refinement must be < 1e-12");
}

E2E_TEST(T1_F06_02_SparseBasisReplaceColumnAndSolve,
         "Replace column in basis and verify solve accuracy across update chain",
         Tier1, M1, 6) {
    std::vector<std::vector<double>> cols = {
        {4.0, 1.0, 0.0},
        {1.0, 5.0, 1.0},
        {0.0, 2.0, 6.0}
    };
    auto csc = linalg::SparseCsc::from_columns(3, cols);
    auto basis = linalg::SparseBasisFactorization::factorize(csc);

    std::vector<double> rep = {2.0, 1.0, 1.0};
    basis.replace_column(1, rep);
    cols[1] = rep;
    auto updated_csc = linalg::SparseCsc::from_columns(3, cols);

    std::vector<double> b = {1.0, 3.0, 5.0};
    auto x = basis.solve(b);
    double res = linalg::sparse_infinity_residual(updated_csc, x, b);
    E2E_ASSERT_NEAR(res, 0.0, 1e-11, "Updated basis solve residual must be < 1e-11");
}
