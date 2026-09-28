#include "test_tier1_m1_features_fixtures.hpp"
E2E_TEST(T1_F06_03_SparseBasisStatisticsTracking,
         "Verify SparseBasisStatistics records updates and refactorizations",
         Tier1, M1, 6) {
    std::vector<std::vector<double>> cols = {
        {2.0, 0.0},
        {0.0, 3.0}
    };
    auto csc = linalg::SparseCsc::from_columns(2, cols);
    linalg::SparseBasisOptions opts;
    opts.maximum_updates = 2;
    auto basis = linalg::SparseBasisFactorization::factorize(csc, opts);

    E2E_ASSERT(basis.statistics().refactorizations == 1, "Initial factorization recorded");
    basis.replace_column(0, {3.0, 1.0});
    E2E_ASSERT(basis.statistics().updates == 1, "Update count recorded");
    E2E_ASSERT(basis.statistics().current_update_chain == 1, "Current chain length 1");
}

E2E_TEST(T1_F06_04_SparseBasisRefactorizeReset,
         "Refactorization resets current update chain to zero",
         Tier1, M1, 6) {
    std::vector<std::vector<double>> cols = {
        {2.0, 0.0},
        {0.0, 3.0}
    };
    auto csc = linalg::SparseCsc::from_columns(2, cols);
    auto basis = linalg::SparseBasisFactorization::factorize(csc);
    basis.replace_column(0, {4.0, 0.5});
    E2E_ASSERT(basis.statistics().current_update_chain == 1, "Chain has 1 update");

    basis.refactorize();
    E2E_ASSERT(basis.statistics().current_update_chain == 0, "Chain reset to 0 after refactor");
    E2E_ASSERT(basis.statistics().refactorizations == 2, "Refactorizations incremented to 2");
}

E2E_TEST(T1_F06_05_SparseBasisRefinementOptions,
         "Verify refinement options triggers and thresholds are configurable",
         Tier1, M1, 6) {
    linalg::SparseBasisOptions opts;
    opts.maximum_refinement_steps = 4;
    opts.refinement_trigger_updates = 8;
    opts.refinement_trigger_growth = 50.0;
    E2E_ASSERT(opts.maximum_refinement_steps == 4, "Refinement steps 4");
    E2E_ASSERT(opts.refinement_trigger_updates == 8, "Trigger updates 8");
    E2E_ASSERT_NEAR(opts.refinement_trigger_growth, 50.0, 1e-12, "Trigger growth 50.0");
}

// ---------------------------------------------------------------------------
// Feature 7: Structured Numerical Diagnostics & Verification
// ---------------------------------------------------------------------------

E2E_TEST(T1_F07_01_SolveModelPrimalVerificationReport,
         "api::solve_model populates valid primal verification report",
         Tier1, M1, 7) {
    model::Model model;
    model.name = "API_PRIMA";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {1.0, 1.0};
    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::finite(2.0)};
    model.row_upper = {model::Bound::finite(2.0)};
    model.row_name = {"R1"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(5.0), model::Bound::finite(5.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    auto res = api::solve_model(model);
    E2E_ASSERT(res.status == lp::reference::SolveStatus::optimal, "Optimal solve");
    E2E_ASSERT(res.primal_report.passed, "Primal report passed");
    E2E_ASSERT_NEAR(res.primal_report.maximum_row_violation, 0.0, 1e-6, "Row violation < 1e-6");
}

E2E_TEST(T1_F07_02_SolveModelCanonicalVerificationReport,
         "api::solve_model populates canonical verification report",
         Tier1, M1, 7) {
    model::Model model;
    model.name = "API_CANON";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {-2.0, -1.0};
    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 2.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::negative_infinity()};
    model.row_upper = {model::Bound::finite(6.0)};
    model.row_name = {"LE"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    auto res = api::solve_model(model);
    E2E_ASSERT(res.status == lp::reference::SolveStatus::optimal, "Optimal status");
    E2E_ASSERT(res.canonical_verified, "Canonical verified");
}

E2E_TEST(T1_F07_03_ZeroTrustVerificationFlag,
         "api::solve_model asserts overall verified flag is true on optimal solve",
         Tier1, M1, 7) {
    model::Model model;
    model.name = "API_VERIFIED";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {3.0, 4.0};
    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::finite(1.0)};
    model.row_upper = {model::Bound::positive_infinity()};
    model.row_name = {"GE"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    auto res = api::solve_model(model);
    E2E_ASSERT(res.verified, "Overall verified flag must be true");
    E2E_ASSERT(res.original_verified, "Original verified flag must be true");
}

E2E_TEST(T1_F07_04_QpSolutionKktVerification,
         "verify_qp_solution certifies KKT stationarity and feasibility on QP solution",
         Tier1, M1, 7) {
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
    qp.l = {3.0};
    qp.u = {1e20};

    qp::QpOptions opts;
    opts.absolute_tolerance = 1e-6;
    opts.relative_tolerance = 1e-6;
    auto sol = qp::solve_qp(qp, opts);
    auto rep = qp::verify_qp_solution(qp, sol, 1e-4);
    E2E_ASSERT(rep.passed, "QP verification report passed");
    E2E_ASSERT_NEAR(rep.maximum_primal_violation, 0.0, 1e-3, "QP primal residual < 1e-3");
    E2E_ASSERT_NEAR(rep.maximum_dual_violation, 0.0, 1e-3, "QP dual residual < 1e-3");
}

E2E_TEST(T1_F07_05_SolveResultDimensionsPopulated,
         "SolveResult records model dimensions and non-zero counts faithfully",
         Tier1, M1, 7) {
    model::Model model;
    model.name = "DIMS";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {1.0, 2.0, 3.0};
    model::SparseMatrixBuilder builder(2, 3);
    builder.add(0, 0, 1.0); builder.add(0, 1, 2.0);
    builder.add(1, 1, 3.0); builder.add(1, 2, 4.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::finite(1.0), model::Bound::finite(1.0)};
    model.row_upper = {model::Bound::finite(1.0), model::Bound::finite(1.0)};
    model.row_name = {"R1", "R2"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(5.0), model::Bound::finite(5.0), model::Bound::finite(5.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2", "X3"};
    model.validate();

    auto res = api::solve_model(model);
    E2E_ASSERT(res.model_rows == 2, "Model rows = 2");
    E2E_ASSERT(res.model_cols == 3, "Model cols = 3");
    E2E_ASSERT(res.model_nnz == 4, "Model nnz = 4");
    E2E_ASSERT(res.runtime_ms >= 0.0, "Runtime ms is non-negative");
}
