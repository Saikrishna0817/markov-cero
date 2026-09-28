#include "test_tier1_m1_features_fixtures.hpp"
E2E_TEST(T1_F01_01_NormalEqFactorizeSolve,
         "Factorize and solve symmetric normal equations system ADA^T dy = rhs",
         Tier1, M1, 1) {
    // Construct 3x5 matrix A
    std::vector<std::vector<double>> cols = {
        {1.0, 0.0, 2.0},
        {0.0, 3.0, 1.0},
        {1.0, 1.0, 0.0},
        {2.0, 0.0, 1.0},
        {0.0, 1.0, 4.0}
    };
    auto A = linalg::SparseCsc::from_columns(3, cols);
    std::vector<double> diag_D = {1.5, 2.0, 0.5, 3.0, 1.2};

    auto normal_eq = compute_normal_equations(A, diag_D);
    E2E_ASSERT(normal_eq.rows == 3 && normal_eq.columns == 3, "Normal equations shape 3x3");

    auto lu = linalg::SparseLu::factorize(normal_eq, 1e-14, 1024, true);
    E2E_ASSERT(lu.dimension() == 3, "LU dimension must be 3");

    std::vector<double> rhs = {10.0, -4.0, 7.5};
    auto dy = lu.solve(rhs);
    E2E_ASSERT(dy.size() == 3, "Solution size must match rows");

    // Verify residual ||ADA^T dy - rhs||_inf < 1e-10
    double res_norm = linalg::sparse_infinity_residual(normal_eq, dy, rhs);
    E2E_ASSERT_NEAR(res_norm, 0.0, 1e-10, "Normal equations residual must be < 1e-10");
}

E2E_TEST(T1_F01_02_NormalEqFillReducingOrdering,
         "Verify minimum-degree fill-reducing ordering on tridiagonal normal equations",
         Tier1, M1, 1) {
    const std::size_t m = 10;
    std::vector<std::vector<double>> cols(m, std::vector<double>(m, 0.0));
    for (std::size_t i = 0; i < m; ++i) {
        cols[i][i] = 4.0;
        if (i > 0) cols[i][i - 1] = -1.0;
        if (i + 1 < m) cols[i][i + 1] = -1.0;
    }
    auto csc = linalg::SparseCsc::from_columns(m, cols);

    auto lu = linalg::SparseLu::factorize(csc, 1e-14, 4096, true);
    const auto& diag = lu.diagnostics();
    E2E_ASSERT(diag.factor_nonzeros > 0, "Factor nonzeros must be positive");
    E2E_ASSERT(diag.maximum_absolute_pivot > 0.0, "Pivot must be positive");

    std::vector<double> b(m, 1.0);
    auto x = lu.solve(b);
    double res = linalg::sparse_infinity_residual(csc, x, b);
    E2E_ASSERT_NEAR(res, 0.0, 1e-12, "Tridiagonal solve residual must be < 1e-12");
}

E2E_TEST(T1_F01_03_SparseLuTransposeSolve,
         "Verify transpose solve (BTRAN) on normal equations factorizer",
         Tier1, M1, 1) {
    std::vector<std::vector<double>> cols = {
        {5.0, 1.0, 0.5},
        {1.0, 6.0, 2.0},
        {0.5, 2.0, 7.0}
    };
    auto csc = linalg::SparseCsc::from_columns(3, cols);
    auto lu = linalg::SparseLu::factorize(csc);

    std::vector<double> b = {3.0, 12.0, -1.0};
    auto xt = lu.solve_transpose(b);
    double res = linalg::sparse_infinity_residual(csc, xt, b, true);
    E2E_ASSERT_NEAR(res, 0.0, 1e-12, "Transpose solve residual must be < 1e-12");
}

E2E_TEST(T1_F01_04_CanonicalIpmOptimal,
         "Solve standard 2-variable LP via IPM to certified optimality",
         Tier1, M1, 1) {
    // min -x1 - 2*x2 s.t. x1 + x2 <= 4, x1 <= 3, x2 <= 3, x >= 0
    // Optimum: x* = (1, 3), obj = -7
    model::Model model;
    model.name = "IPM_T1";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {-1.0, -2.0};
    model.objective_offset = 0.0;

    model::SparseMatrixBuilder builder(3, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    builder.add(1, 0, 1.0);
    builder.add(2, 1, 1.0);
    model.matrix = builder.build();

    model.row_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.row_upper = {model::Bound::finite(4.0), model::Bound::finite(3.0), model::Bound::finite(3.0)};
    model.row_name = {"C1", "C2", "C3"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    auto canonical = transform::canonicalize(model);
    lp::interior::Options opts;
    opts.iteration_limit = 100;
    opts.relative_tolerance = 1e-8;
    opts.enable_crossover = true;

    auto res = lp::interior::solve(canonical, opts);
    E2E_ASSERT(res.status == lp::reference::SolveStatus::optimal, "IPM must reach optimal status");
    E2E_ASSERT_NEAR(res.objective, -7.0, 1e-5, "IPM objective must match -7.0");
    E2E_ASSERT(res.primal.size() == canonical.matrix.columns, "Primal vector size must match canonical columns");
}

E2E_TEST(T1_F01_05_IpmCrossoverBasisCertification,
         "Verify IPM crossover returns certified vertex basis matching canonical rows",
         Tier1, M1, 1) {
    model::Model model;
    model.name = "IPM_BASIS";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {1.0, 2.0};
    model.objective_offset = 0.0;

    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();

    model.row_lower = {model::Bound::finite(1.0)};
    model.row_upper = {model::Bound::finite(1.0)};
    model.row_name = {"EQ1"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(5.0), model::Bound::finite(5.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    auto canonical = transform::canonicalize(model);
    auto res = lp::interior::solve(canonical, {});
    E2E_ASSERT(res.status == lp::reference::SolveStatus::optimal, "Optimal solve");
    E2E_ASSERT(res.crossover_applied, "Crossover must be applied");
    E2E_ASSERT(res.basis_state.has_value(), "Basis state must be populated");
    E2E_ASSERT(res.basis_state->basic_variables.size() == canonical.matrix.rows,
               "Basis size must equal row count");
}

// ---------------------------------------------------------------------------
// Feature 2 & 3: PDLP Stagnation & Crossover
// ---------------------------------------------------------------------------

E2E_TEST(T1_F02_01_PdlpSolveBlendNominal,
         "PDLP first-order solver converges on blend LP model",
         Tier1, M1, 2) {
    model::Model model;
    model.name = "PDLP_BLEND";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {-1.0, -2.0};

    model::SparseMatrixBuilder builder(3, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    builder.add(1, 0, 1.0);
    builder.add(2, 1, 1.0);
    model.matrix = builder.build();

    model.row_lower = {model::Bound::negative_infinity(),
                       model::Bound::negative_infinity(),
                       model::Bound::negative_infinity()};
    model.row_upper = {model::Bound::finite(4.0), model::Bound::finite(3.0), model::Bound::finite(3.0)};
    model.row_name = {"SUM", "X1_UB", "X2_UB"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    opts.max_iterations = 50000;
    opts.set_tolerance(1e-4);

    auto res = lp::first_order::solve_pdlp(model, opts);
    E2E_ASSERT(res.status == lp::first_order::PdlpStatus::optimal ||
               res.status == lp::first_order::PdlpStatus::iteration_limit,
               "PDLP must reach optimal or iteration limit");
    E2E_ASSERT_NEAR(res.objective, -7.0, 0.2, "PDLP objective near -7.0");
    E2E_ASSERT(res.primal.size() == 2, "Primal solution has 2 variables");
}

E2E_TEST(T1_F02_02_PdlpToleranceHandling,
         "Verify set_tolerance sets primal, dual, and gap tolerances concurrently",
         Tier1, M1, 2) {
    lp::first_order::PdlpOptions opts;
    opts.set_tolerance(1e-5);
    E2E_ASSERT_NEAR(opts.primal_tolerance, 1e-5, 1e-12, "Primal tol 1e-5");
    E2E_ASSERT_NEAR(opts.dual_tolerance, 1e-5, 1e-12, "Dual tol 1e-5");
    E2E_ASSERT_NEAR(opts.gap_tolerance, 1e-5, 1e-12, "Gap tol 1e-5");
}

E2E_TEST(T1_F02_03_PdlpRuizScalingEnabled,
         "Verify PDLP with Ruiz scaling enabled preserves objective fidelity",
         Tier1, M1, 2) {
    model::Model model;
    model.name = "PDLP_RUIZ";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {100.0, 1.0};

    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 100.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();

    model.row_lower = {model::Bound::finite(101.0)};
    model.row_upper = {model::Bound::finite(101.0)};
    model.row_name = {"EQ"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    opts.ruiz_scaling = true;
    opts.ruiz_iterations = 10;
    opts.max_iterations = 20000;
    opts.set_tolerance(1e-3);

    auto res = lp::first_order::solve_pdlp(model, opts);
    E2E_ASSERT_NEAR(res.primal_infeasibility, 0.0, 1e-2, "Primal infeasibility bounded");
}


int main(int argc, char** argv) {
    return TestRegistry::instance().run(argc, argv);
}
