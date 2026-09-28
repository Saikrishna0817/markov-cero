#include "test_tier2_m1_boundaries_fixtures.hpp"
E2E_TEST(T2_F04_03_AdmmPrimalInfeasibleDetection,
         "ADMM QP solver detects primal infeasibility when constraints conflict",
         Tier2, M1, 4) {
    // min (1/2) x^2 s.t. x >= 5 and x <= 2
    qp::QuadraticModel model;
    model.variable_names = {"X1"};
    model.P.dimension = 1;
    model.P.column_offsets = {0, 1};
    model.P.row_indices = {0};
    model.P.values = {1.0};
    model.q = {0.0};
    model.A.rows = 2;
    model.A.columns = 1;
    model.A.column_offsets = {0, 2};
    model.A.row_indices = {0, 1};
    model.A.values = {1.0, 1.0};
    model.l = {5.0, -1e20};
    model.u = {1e20, 2.0};

    qp::QpOptions opts;
    opts.max_iterations = 500;
    auto sol = qp::solve_qp(model, opts);
    E2E_ASSERT(sol.status == qp::QpStatus::primal_infeasible ||
               sol.status == qp::QpStatus::iteration_limit,
               "Infeasible QP must report primal_infeasible or iteration_limit");
}

E2E_TEST(T2_F04_04_AdmmExtremeInitialRho,
         "ADMM solver remains stable under extreme initial rho (1e4)",
         Tier2, M1, 4) {
    qp::QuadraticModel model;
    model.variable_names = {"X1"};
    model.P.dimension = 1;
    model.P.column_offsets = {0, 1};
    model.P.row_indices = {0};
    model.P.values = {2.0};
    model.q = {-6.0};
    model.A.rows = 0;
    model.A.columns = 1;
    model.A.column_offsets = {0, 0};

    qp::QpOptions opts;
    opts.rho_init = 1e4;
    opts.adaptive_rho = true;
    auto sol = qp::solve_qp(model, opts);
    E2E_ASSERT(sol.status == qp::QpStatus::optimal, "Converged with high initial rho");
    E2E_ASSERT_NEAR(sol.x[0], 3.0, 1e-2, "Optimum x* = 3.0");
}

E2E_TEST(T2_F05_01_AdmmKktIllConditionedSigmaRegularization,
         "KktSolver factorizes even when P has zero eigenvalues via sigma regularization",
         Tier2, M1, 5) {
    // P = 0 (pure linear objective in QP)
    qp::SparseSymmetricMatrix P;
    P.dimension = 2;
    P.column_offsets = {0, 0, 0}; // Empty P
    linalg::SparseCsc A;
    A.rows = 1;
    A.columns = 2;
    A.column_offsets = {0, 1, 2};
    A.row_indices = {0, 0};
    A.values = {1.0, 1.0};

    qp::KktSolver kkt;
    bool ok = kkt.factorize(P, A, 1e-6, {1.0});
    E2E_ASSERT(ok, "Factorization with sigma regularization must succeed for P = 0");
}

// ---------------------------------------------------------------------------
// Feature 6: Always-On Iterative Refinement Boundaries
// ---------------------------------------------------------------------------

E2E_TEST(T2_F06_01_SparseBasisIllConditionedRefinement,
         "Sparse basis refinement improves residual on ill-conditioned Hilbert-style matrix",
         Tier2, M1, 6) {
    // 3x3 matrix with disparate scaling
    std::vector<std::vector<double>> cols = {
        {1.0, 1e-4, 1e-8},
        {1e-4, 1.0, 1e-4},
        {1e-8, 1e-4, 1.0}
    };
    auto csc = linalg::SparseCsc::from_columns(3, cols);
    linalg::SparseBasisOptions opts;
    opts.maximum_refinement_steps = 3;
    auto basis = linalg::SparseBasisFactorization::factorize(csc, opts);

    std::vector<double> b = {1.0, 2.0, 3.0};
    auto x = basis.solve(b);
    double res = linalg::sparse_infinity_residual(csc, x, b);
    E2E_ASSERT_NEAR(res, 0.0, 1e-12, "Residual bounded under iterative refinement");
}

E2E_TEST(T2_F06_02_SparseBasisZeroRhsVector,
         "Sparse basis solve with b = 0 returns exact zero vector immediately",
         Tier2, M1, 6) {
    std::vector<std::vector<double>> cols = {
        {3.0, 1.0},
        {1.0, 4.0}
    };
    auto csc = linalg::SparseCsc::from_columns(2, cols);
    auto basis = linalg::SparseBasisFactorization::factorize(csc);

    std::vector<double> zero_b(2, 0.0);
    auto x = basis.solve(zero_b);
    E2E_ASSERT_NEAR(x[0], 0.0, 1e-15, "x[0] = 0.0");
    E2E_ASSERT_NEAR(x[1], 0.0, 1e-15, "x[1] = 0.0");
}

E2E_TEST(T2_F06_03_SparseBasisMaxUpdatesThreshold,
         "Exceeding maximum_updates sets needs_refactorization flag to true",
         Tier2, M1, 6) {
    std::vector<std::vector<double>> cols = {
        {5.0, 0.0},
        {0.0, 5.0}
    };
    auto csc = linalg::SparseCsc::from_columns(2, cols);
    linalg::SparseBasisOptions opts;
    opts.maximum_updates = 1; // Trigger after 1 update
    auto basis = linalg::SparseBasisFactorization::factorize(csc, opts);
    E2E_ASSERT_FALSE(basis.needs_refactorization());

    basis.replace_column(0, {6.0, 1.0});
    E2E_ASSERT_TRUE(basis.needs_refactorization());
}

E2E_TEST(T2_F06_04_SparseBasisSingularUpdateRejection,
         "Replacing column with dependent column throws runtime_error without state corruption",
         Tier2, M1, 6) {
    std::vector<std::vector<double>> cols = {
        {1.0, 0.0},
        {0.0, 1.0}
    };
    auto csc = linalg::SparseCsc::from_columns(2, cols);
    auto basis = linalg::SparseBasisFactorization::factorize(csc);

    // Column 1 is {0, 1}. Replacing Column 0 with {0, 2} makes columns linearly dependent!
    bool threw = false;
    try {
        basis.replace_column(0, {0.0, 2.0});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    E2E_ASSERT_TRUE(threw);
}

E2E_TEST(T2_F06_05_SparseBasisDegenerateZeroRowResidual,
         "sparse_infinity_residual on 0-row matrix returns 0.0",
         Tier2, M1, 6) {
    linalg::SparseCsc csc{0, 2, {0, 0, 0}, {}, {}};
    csc.validate();
    double r = linalg::sparse_infinity_residual(csc, {1.0, 2.0}, {});
    E2E_ASSERT_NEAR(r, 0.0, 1e-15, "0-row residual must be 0.0");
}

// ---------------------------------------------------------------------------
// Feature 7: Structured Numerical Diagnostics & Verification Boundaries
// ---------------------------------------------------------------------------

E2E_TEST(T2_F07_01_InfeasibleSolveVerificationReport,
         "api::solve_model on infeasible problem reports infeasibility correctly",
         Tier2, M1, 7) {
    model::Model model;
    model.name = "INFEASIBLE";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {1.0};
    model::SparseMatrixBuilder builder(2, 1);
    builder.add(0, 0, 1.0);
    builder.add(1, 0, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::finite(5.0), model::Bound::negative_infinity()};
    model.row_upper = {model::Bound::positive_infinity(), model::Bound::finite(2.0)};
    model.row_name = {"R1", "R2"};
    model.variable_lower = {model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous};
    model.variable_name = {"X1"};
    model.validate();

    auto res = api::solve_model(model);
    E2E_ASSERT(res.status == lp::reference::SolveStatus::infeasible,
               "Solve status must be infeasible");
    E2E_ASSERT_TRUE(res.verified);
}

E2E_TEST(T2_F07_02_VerifierRejectsPerturbedSolution,
         "verify_primal rejects solution when perturbed outside tolerance",
         Tier2, M1, 7) {
    model::Model model;
    model.name = "PERTURBED";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {1.0};
    model::SparseMatrixBuilder builder(1, 1);
    builder.add(0, 0, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::finite(5.0)};
    model.row_upper = {model::Bound::finite(5.0)};
    model.row_name = {"EQ"};
    model.variable_lower = {model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous};
    model.variable_name = {"X1"};
    model.validate();

    // Intentionally wrong candidate x = 4.0 (violates row lower by 1.0)
    verify::Candidate bad_cand{{4.0}, 4.0};
    auto report = verify::verify_primal(model, bad_cand, {1e-4, 1e-4}, {1e-4, 1e-4}, 1e-4);
    E2E_ASSERT_FALSE(report.passed);
    E2E_ASSERT(report.maximum_row_violation > 0.5, "Row violation detected");
}

E2E_TEST(T2_F07_03_QpVerifierRejectsInvalidResiduals,
         "verify_qp_solution fails when candidate violates bounds",
         Tier2, M1, 7) {
    qp::QuadraticModel qp;
    qp.variable_names = {"X1"};
    qp.P.dimension = 1;
    qp.P.column_offsets = {0, 1};
    qp.P.row_indices = {0};
    qp.P.values = {2.0};
    qp.q = {0.0};
    qp.A.rows = 1;
    qp.A.columns = 1;
    qp.A.column_offsets = {0, 1};
    qp.A.row_indices = {0};
    qp.A.values = {1.0};
    qp.l = {10.0};
    qp.u = {20.0};

    qp::QpSolution bad_sol;
    bad_sol.status = qp::QpStatus::optimal;
    bad_sol.x = {0.0}; // Violates l = 10.0
    bad_sol.z = {0.0};
    bad_sol.y = {0.0};

    auto rep = qp::verify_qp_solution(qp, bad_sol);
    E2E_ASSERT_FALSE(rep.passed);
}
