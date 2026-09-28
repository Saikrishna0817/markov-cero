// markov-cero E2E Test Suite: Tier 3 - Milestone 1 Cross-Feature Combinations
// Tests pairwise interactions between interacting Milestone 1 features.

#include "markov_cero/api/solve.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/lp/interior/ipm.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/presolve/presolve.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/kkt.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/scale/ruiz_scaling.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include "e2e_test_framework.hpp"

#include <cmath>
#include <vector>

using namespace markov_cero;
using namespace markov_cero::testing;

E2E_TEST(T3_PAIR_01_SparseLuWithIterativeRefinement,
         "Pairwise F01 + F06: SparseLU normal equations with always-on iterative refinement",
         Tier3, M1, 1) {
    // Normal equations from 3x4 system
    std::vector<std::vector<double>> cols = {
        {2.0, 1.0, 0.0},
        {1.0, 3.0, 1.0},
        {0.0, 1.0, 2.0},
        {1.0, 0.0, 1.0}
    };
    auto A = linalg::SparseCsc::from_columns(3, cols);
    std::vector<double> diag_D = {1.0, 2.0, 0.5, 1.5};

    // Form ADA^T
    std::vector<std::vector<double>> B_dense(3, std::vector<double>(3, 0.0));
    for (std::size_t k = 0; k < 4; ++k) {
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) {
                B_dense[i][j] += diag_D[k] * cols[k][i] * cols[k][j];
            }
        }
    }
    auto B = linalg::SparseCsc::from_columns(3, B_dense);

    linalg::SparseBasisOptions opts;
    opts.maximum_refinement_steps = 3;
    auto basis = linalg::SparseBasisFactorization::factorize(B, opts);

    std::vector<double> rhs = {5.0, 8.0, 4.0};
    auto dy = basis.solve(rhs);
    double res = linalg::sparse_infinity_residual(B, dy, rhs);
    E2E_ASSERT_NEAR(res, 0.0, 1e-12, "Iterative refinement residual on normal equations");
}

E2E_TEST(T3_PAIR_03_PdlpWithZeroTrustPrimalVerification,
         "Pairwise F02/03 + F07: PDLP first-order solve verified by zero-trust primal verifier",
         Tier3, M1, 2) {
    model::Model model;
    model.name = "PAIR_PDLP_VERIFY";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {-2.0, -3.0};
    model::SparseMatrixBuilder builder(2, 2);
    builder.add(0, 0, 1.0); builder.add(0, 1, 2.0);
    builder.add(1, 0, 2.0); builder.add(1, 1, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::negative_infinity(), model::Bound::negative_infinity()};
    model.row_upper = {model::Bound::finite(8.0), model::Bound::finite(7.0)};
    model.row_name = {"C1", "C2"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    lp::first_order::PdlpOptions opts;
    opts.set_tolerance(1e-4);
    auto res = lp::first_order::solve_pdlp(model, opts);

    verify::Candidate cand{res.primal, res.objective};
    auto report = verify::verify_primal(model, cand, {1e-4, 1e-4}, {1e-4, 1e-4});
    E2E_ASSERT(report.passed, "Primal verification passes on PDLP solution");
}

E2E_TEST(T3_PAIR_05_AdmmAdaptiveRhoWithQpVerifier,
         "Pairwise F04/05 + F07: ADMM adaptive rho QP solve certified by KKT verifier",
         Tier3, M1, 4) {
    qp::QuadraticModel qp;
    qp.variable_names = {"X1", "X2"};
    qp.constraint_names = {"CONSTR"};
    qp.P.dimension = 2;
    qp.P.column_offsets = {0, 1, 2};
    qp.P.row_indices = {0, 1};
    qp.P.values = {2.0, 2.0};
    qp.q = {-2.0, -5.0};
    qp.A.rows = 1;
    qp.A.columns = 2;
    qp.A.column_offsets = {0, 1, 2};
    qp.A.row_indices = {0, 0};
    qp.A.values = {1.0, 2.0};
    qp.l = {-1e20};
    qp.u = {4.0};

    qp::QpOptions opts;
    opts.absolute_tolerance = 1e-6;
    opts.relative_tolerance = 1e-6;
    opts.adaptive_rho = true;
    opts.adaptive_rho_interval = 10;
    auto sol = qp::solve_qp(qp, opts);
    E2E_ASSERT(sol.status == qp::QpStatus::optimal, "ADMM QP optimal");

    auto rep = qp::verify_qp_solution(qp, sol, 1e-4);
    E2E_ASSERT(rep.passed, "KKT verifier passes for adaptive rho solution");
    E2E_ASSERT_NEAR(rep.maximum_primal_violation, 0.0, 1e-3, "Primal residual < 1e-3");
}

E2E_TEST(T3_PAIR_06_RuizScalingWithPresolveAndSolve,
         "Integration: Ruiz scaling combined with presolve and solve_model",
         Tier3, M1, 7) {
    model::Model model;
    model.name = "RUIZ_PRESOLVE_PAIR";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {1000.0, 1.0};
    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1000.0); builder.add(0, 1, 1.0);
    model.matrix = builder.build();
    model.row_lower = {model::Bound::finite(1001.0)};
    model.row_upper = {model::Bound::finite(1001.0)};
    model.row_name = {"EQ"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(10.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    api::SolveOptions opts;
    opts.enable_presolve = true;
    opts.enable_scale = true;
    opts.ruiz_iterations = 10;

    auto res = api::solve_model(model, opts);
    E2E_ASSERT(res.status == lp::reference::SolveStatus::optimal, "Optimal solve");
    E2E_ASSERT(res.verified, "Solution verified under scaling and presolve");
    E2E_ASSERT_NEAR(res.objective, 1001.0, 1e-4, "Objective matches 1001.0");
}

int main(int argc, char** argv) {
    return TestRegistry::instance().run(argc, argv);
}
