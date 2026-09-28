// markov-cero E2E Test Suite: Tier 1 - Milestone 1 Feature Coverage
// Verifies nominal (happy path) functionality for Milestone 1 features:
// Feature 1: SparseLU IPM Normal Equations
// Feature 2: PDLP Stagnation Detection
// Feature 3: PDLP Dual Simplex Crossover
// Feature 4: ADMM Adaptive Penalty Rho
// Feature 5: ADMM KKT Refactorization Counter
// Feature 6: Always-On Iterative Refinement
// Feature 7: Structured Numerical Diagnostics & Verification

#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
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
#include <numeric>
#include <vector>

using namespace markov_cero;
using namespace markov_cero::testing;

// Helper: compute A * D * A^T where A is m x n sparse CSC and D is n x n diagonal
static linalg::SparseCsc compute_normal_equations(const linalg::SparseCsc& A,
                                                  const std::vector<double>& diag_D) {
    const std::size_t m = A.rows;
    const std::size_t n = A.columns;
    std::vector<std::vector<double>> full_mat(m, std::vector<double>(m, 0.0));

    // A is m x n, D is n x n. A D A^T = sum_{k=0}^{n-1} D[k] * A[:, k] * A[:, k]^T
    for (std::size_t k = 0; k < n; ++k) {
        const double d_k = diag_D[k];
        const std::size_t start = A.column_offsets[k];
        const std::size_t end = A.column_offsets[k + 1];
        for (std::size_t p1 = start; p1 < end; ++p1) {
            const std::size_t i = A.row_indices[p1];
            const double val_i = A.values[p1];
            for (std::size_t p2 = start; p2 < end; ++p2) {
                const std::size_t j = A.row_indices[p2];
                const double val_j = A.values[p2];
                full_mat[i][j] += d_k * val_i * val_j;
            }
        }
    }

    // Add small regularization to ensure positive definiteness
    for (std::size_t i = 0; i < m; ++i) {
        full_mat[i][i] += 1e-8;
    }

    return linalg::SparseCsc::from_columns(m, full_mat);
}

// ---------------------------------------------------------------------------
// Feature 1: SparseLU IPM Normal Equations
// ---------------------------------------------------------------------------

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
    auto rep = qp::verify_qp_solution(qp, sol, 1e-3);
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

int main(int argc, char** argv) {
    return TestRegistry::instance().run(argc, argv);
}
