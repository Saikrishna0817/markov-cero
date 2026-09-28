#include "test_tier2_m1_boundaries_fixtures.hpp"
E2E_TEST(T2_F01_01_NormalEqExtremeDiagonalScaling,
         "Normal equations with extreme barrier iterate weights (1e8 and 1e-8)",
         Tier2, M1, 1) {
    // A is 2x2 identity, D has 1e8 and 1e-8. ADA^T = diag(1e8, 1e-8)
    std::vector<std::vector<double>> cols = {
        {1e8, 0.0},
        {0.0, 1e-8}
    };
    auto csc = linalg::SparseCsc::from_columns(2, cols);
    auto lu = linalg::SparseLu::factorize(csc, 1e-15, 1024, false);

    std::vector<double> b = {1e8, 1.0};
    auto x = lu.solve(b);
    E2E_ASSERT_NEAR(x[0], 1.0, 1e-10, "x[0] = 1.0");
    E2E_ASSERT_NEAR(x[1], 1e8, 1e-2, "x[1] = 1e8");

    double res = linalg::sparse_infinity_residual(csc, x, b);
    E2E_ASSERT_NEAR(res, 0.0, 1e-6, "Residual under extreme scaling");
}

E2E_TEST(T2_F01_02_Trivial1x1NormalEquations,
         "Trivial 1x1 normal equations system solve",
         Tier2, M1, 1) {
    std::vector<std::vector<double>> cols = {{42.0}};
    auto csc = linalg::SparseCsc::from_columns(1, cols);
    auto lu = linalg::SparseLu::factorize(csc);

    std::vector<double> b = {84.0};
    auto x = lu.solve(b);
    E2E_ASSERT_NEAR(x[0], 2.0, 1e-12, "1x1 solution exact");
}

E2E_TEST(T2_F01_03_SingularMatrixRejection,
         "Singular matrix in SparseLu factorize throws runtime_error without crash",
         Tier2, M1, 1) {
    // 2x2 singular matrix (second column is multiple of first)
    std::vector<std::vector<double>> cols = {
        {1.0, 2.0},
        {2.0, 4.0}
    };
    auto csc = linalg::SparseCsc::from_columns(2, cols);

    bool threw = false;
    try {
        (void)linalg::SparseLu::factorize(csc, 1e-14);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    E2E_ASSERT_TRUE(threw);
}

E2E_TEST(T2_F01_04_UnsortedCscValidation,
         "SparseCsc validation rejects unsorted or corrupt row indices",
         Tier2, M1, 1) {
    // Row indices not sorted: {1, 0}
    linalg::SparseCsc bad_csc{2, 2, {0, 2, 2}, {1, 0}, {1.0, 2.0}};
    bool threw = false;
    try {
        bad_csc.validate();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    E2E_ASSERT_TRUE(threw);
}

E2E_TEST(T2_F01_05_NonFiniteMatrixRejection,
         "SparseCsc validation rejects NaN and Infinity entries",
         Tier2, M1, 1) {
    bool threw = false;
    try {
        (void)linalg::SparseCsc::from_columns(
            2, {{1.0, 0.0}, {0.0, std::numeric_limits<double>::infinity()}});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    E2E_ASSERT_TRUE(threw);
}

// ---------------------------------------------------------------------------
// Feature 2 & 3: PDLP Stagnation & Crossover Boundaries
// ---------------------------------------------------------------------------

E2E_TEST(T2_F02_01_PdlpIterationLimitExit,
         "PDLP with iteration_limit=5 exits cleanly with iteration_limit status",
         Tier2, M1, 2) {
    model::Model model;
    model.name = "PDLP_LIMIT";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {-1.0, -2.0};
    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::negative_infinity()};
    model.row_upper = {model::Bound::finite(4.0)};
    model.row_name = {"C1"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    opts.max_iterations = 5;
    auto res = lp::first_order::solve_pdlp(model, opts);

    E2E_ASSERT(res.status == lp::first_order::PdlpStatus::iteration_limit,
               "Status must be iteration_limit");
    E2E_ASSERT(res.iterations <= 5, "Iterations must not exceed limit");
}

E2E_TEST(T2_F02_02_PdlpZeroObjectiveFeasibility,
         "PDLP solve on pure feasibility problem (zero objective c = 0)",
         Tier2, M1, 2) {
    model::Model model;
    model.name = "PDLP_ZERO_OBJ";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {0.0, 0.0};
    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::finite(3.0)};
    model.row_upper = {model::Bound::finite(3.0)};
    model.row_name = {"C1"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(5.0), model::Bound::finite(5.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    opts.set_tolerance(1e-4);
    auto res = lp::first_order::solve_pdlp(model, opts);
    E2E_ASSERT_NEAR(res.objective, 0.0, 1e-4, "Objective must remain 0.0");
    E2E_ASSERT_NEAR(res.primal[0] + res.primal[1], 3.0, 1e-2, "Constraint satisfied");
}

E2E_TEST(T2_F02_03_PdlpInfiniteBoundsClamp,
         "PDLP handles variables with positive/negative infinite bounds correctly",
         Tier2, M1, 2) {
    model::Model model;
    model.name = "PDLP_INF_BOUNDS";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {1.0};
    model::SparseMatrixBuilder builder(1, 1);
    builder.add(0, 0, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::finite(10.0)};
    model.row_upper = {model::Bound::positive_infinity()};
    model.row_name = {"GE"};
    model.variable_lower = {model::Bound::negative_infinity()};
    model.variable_upper = {model::Bound::positive_infinity()};
    model.variable_type = {model::VariableType::continuous};
    model.variable_name = {"X1"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    opts.set_tolerance(1e-3);
    auto res = lp::first_order::solve_pdlp(model, opts);
    E2E_ASSERT_NEAR(res.objective, 10.0, 0.5, "Optimal x near 10.0");
}

E2E_TEST(T2_F03_01_PdlpEmptyProblem,
         "PDLP on model with zero constraints converges immediately",
         Tier2, M1, 3) {
    model::Model model;
    model.name = "PDLP_EMPTY_CONSTR";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {2.0};
    model::SparseMatrixBuilder builder(0, 1);
    model.matrix = builder.build();
    model.variable_lower = {model::Bound::finite(3.0)};
    model.variable_upper = {model::Bound::finite(8.0)};
    model.variable_type = {model::VariableType::continuous};
    model.variable_name = {"X1"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    auto res = lp::first_order::solve_pdlp(model, opts);
    E2E_ASSERT(res.status == lp::first_order::PdlpStatus::optimal, "m=0 optimal");
    E2E_ASSERT_NEAR(res.objective, 6.0, 1e-12, "box-only objective includes the minimizing variable bound");
}

E2E_TEST(T2_F03_02_PdlpHighToleranceConvergence,
         "PDLP with loose tolerance (1e-2) converges in very few iterations",
         Tier2, M1, 3) {
    model::Model model;
    model.name = "PDLP_LOOSE";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {-1.0, -1.0};
    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::negative_infinity()};
    model.row_upper = {model::Bound::finite(5.0)};
    model.row_name = {"LE"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    opts.set_tolerance(1e-2);
    auto res = lp::first_order::solve_pdlp(model, opts);
    E2E_ASSERT(res.status == lp::first_order::PdlpStatus::optimal, "Converged with loose tolerance");
    E2E_ASSERT(res.iterations < 2000, "Iterations must be low with loose tolerance");
}

// ---------------------------------------------------------------------------
// Feature 4 & 5: ADMM Adaptive Rho & Refactorization Boundaries
// ---------------------------------------------------------------------------

E2E_TEST(T2_F04_01_AdmmZeroVariables,
         "ADMM QP solver with zero variables returns optimal status immediately",
         Tier2, M1, 4) {
    qp::QuadraticModel model;
    model.objective_offset = 15.0;
    auto sol = qp::solve_qp(model);
    E2E_ASSERT(sol.status == qp::QpStatus::optimal, "Zero variables optimal");
    E2E_ASSERT_NEAR(sol.objective_value, 15.0, 1e-12, "Objective equals offset");
}

E2E_TEST(T2_F04_02_AdmmNonConvexDetection,
         "ADMM QP solver flags non-convex quadratic objective P < 0 gracefully",
         Tier2, M1, 4) {
    qp::QuadraticModel model;
    model.variable_names = {"X1"};
    model.P.dimension = 1;
    model.P.column_offsets = {0, 1};
    model.P.row_indices = {0};
    model.P.values = {-2.0}; // Negative definite!
    model.q = {1.0};
    model.A.rows = 0;
    model.A.columns = 1;
    model.A.column_offsets = {0, 0};

    auto sol = qp::solve_qp(model);
    E2E_ASSERT(sol.status == qp::QpStatus::non_convex, "Must flag non-convex");
}


int main(int argc, char** argv) {
    return TestRegistry::instance().run(argc, argv);
}
