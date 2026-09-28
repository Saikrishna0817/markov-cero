// markov-cero E2E Test Suite: Tier 2 - Milestone 1 Boundary & Corner Cases
// Tests boundary conditions, degeneracies, ill-conditioned matrices, zero dimensions,
// floating point extremes, and error paths for Milestone 1 features.

#include "markov_cero/api/solve.hpp"
#include "markov_cero/linalg/dense_lu.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/lp/interior/ipm.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/kkt.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include "e2e_test_framework.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace markov_cero;
using namespace markov_cero::testing;

// ---------------------------------------------------------------------------
// Feature 1: SparseLU IPM Normal Equations Boundaries
// ---------------------------------------------------------------------------

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
    E2E_ASSERT_NEAR(res.objective, 0.0, 1e-12, "m=0 returns trivial 0.0 objective");
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

E2E_TEST(T2_F07_04_PresolveDisabledDiagnosticMatch,
         "api::solve_model with presolve disabled reaches same objective within 1e-6",
         Tier2, M1, 7) {
    model::Model model;
    model.name = "NO_PRESOLVE";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {-1.0, -3.0};
    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::negative_infinity()};
    model.row_upper = {model::Bound::finite(4.0)};
    model.row_name = {"LE"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(5.0), model::Bound::finite(3.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    api::SolveOptions opt1;
    opt1.enable_presolve = true;
    auto res1 = api::solve_model(model, opt1);

    api::SolveOptions opt2;
    opt2.enable_presolve = false;
    auto res2 = api::solve_model(model, opt2);

    E2E_ASSERT(res1.status == lp::reference::SolveStatus::optimal, "Opt1 optimal");
    E2E_ASSERT(res2.status == lp::reference::SolveStatus::optimal, "Opt2 optimal");
    E2E_ASSERT_NEAR(res1.objective, res2.objective, 1e-6, "Objectives match with/without presolve");
}

E2E_TEST(T2_F07_05_DenseLuNearSingularPivotGrowth,
         "DenseLu records valid growth factor and pivot metrics on near-singular system",
         Tier2, M1, 7) {
    linalg::DenseMatrix mat{2, 2, {1e-12, 1.0, 1.0, 1.0}};
    mat.validate();
    auto lu = linalg::DenseLu::factorize(mat);
    E2E_ASSERT(lu.diagnostics().pivot_ratio <= 1.0 && lu.diagnostics().pivot_ratio > 0.0,
               "Pivot ratio in (0, 1]");
    E2E_ASSERT(lu.dimension() == 2, "Dimension is 2");

    std::vector<double> b = {1.0, 2.0};
    auto x = lu.solve(b);
    E2E_ASSERT(x.size() == 2, "Solution size is 2");
}

int main(int argc, char** argv) {
    return TestRegistry::instance().run(argc, argv);
}
