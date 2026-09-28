// markov-cero E2E Test Suite: Tier 4 - Milestone 1 Real-World Application Scenarios
// Tests realistic domain models (blending, portfolio risk, multi-period supply chain, power dispatch).

#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/verifier.hpp"

#include "e2e_test_framework.hpp"

#include <cmath>
#include <vector>

using namespace markov_cero;
using namespace markov_cero::testing;

E2E_TEST(T4_APP_01_CrudeOilBlendingNominal,
         "Domain Scenario 1: Crude oil blending with octane and sulfur quality constraints",
         Tier4, M1, 7) {
    // 2 Crudes (C1, C2) blended into 100 barrels of regular gasoline.
    // C1: cost $40/bbl, octane 88, sulfur 0.02%
    // C2: cost $50/bbl, octane 94, sulfur 0.01%
    // Gasoline requirements:
    // Volume = 100
    // Octane >= 90
    // Sulfur <= 0.015%
    // Variables: x1 (C1 volume), x2 (C2 volume)
    // min 40 x1 + 50 x2
    // s.t. x1 + x2 = 100
    //      88 x1 + 94 x2 >= 9000  (octane)
    //      0.02 x1 + 0.01 x2 <= 1.5 (sulfur)
    //      x1, x2 >= 0
    model::Model model;
    model.name = "CRUDE_BLENDING";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {40.0, 50.0};

    model::SparseMatrixBuilder builder(3, 2);
    // Volume
    builder.add(0, 0, 1.0); builder.add(0, 1, 1.0);
    // Octane
    builder.add(1, 0, 88.0); builder.add(1, 1, 94.0);
    // Sulfur
    builder.add(2, 0, 0.02); builder.add(2, 1, 0.01);
    model.matrix = builder.build();

    model.row_lower = {model::Bound::finite(100.0), model::Bound::finite(9000.0), model::Bound::negative_infinity()};
    model.row_upper = {model::Bound::finite(100.0), model::Bound::positive_infinity(), model::Bound::finite(1.5)};
    model.row_name = {"VOL", "OCTANE", "SULFUR"};
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(100.0), model::Bound::finite(100.0)};
    model.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    model.variable_name = {"CRUDE_1", "CRUDE_2"};
    model.validate();

    auto res = api::solve_model(model);
    E2E_ASSERT(res.status == lp::reference::SolveStatus::optimal, "Crude blending optimal");
    E2E_ASSERT(res.verified, "Crude blending solution certified");

    // Theoretical optimum: x1 = 50.0, x2 = 50.0, obj = 4500.0
    E2E_ASSERT_NEAR(res.primal[0] + res.primal[1], 100.0, 1e-4, "Total volume = 100 bbl");
    E2E_ASSERT(88.0 * res.primal[0] + 94.0 * res.primal[1] >= 8999.9, "Octane spec met");
    E2E_ASSERT(0.02 * res.primal[0] + 0.01 * res.primal[1] <= 1.5001, "Sulfur spec met");
    E2E_ASSERT_NEAR(res.objective, 4500.0, 1.0, "Cost matches theoretical minimum");
}

E2E_TEST(T4_APP_02_MarkowitzPortfolioAllocation,
         "Domain Scenario 2: Mean-variance quadratic portfolio optimization",
         Tier4, M1, 4) {
    const std::string mps =
        "NAME PORTFOLIO\n"
        "ROWS\n"
        " N RISK\n"
        " E BUDGET\n"
        " G RETURN\n"
        "COLUMNS\n"
        " X1 BUDGET 1 RETURN 0.10\n"
        " X2 BUDGET 1 RETURN 0.15\n"
        "RHS\n"
        " RHS1 BUDGET 1 RETURN 0.12\n"
        "QUADOBJ\n"
        " X1 X1 4\n"
        " X1 X2 1\n"
        " X2 X2 9\n"
        "ENDATA\n";

    const auto model = io::parse_mps_string(mps);
    const auto qp = qp::make_quadratic_model(model);

    qp::QpOptions opts;
    opts.absolute_tolerance = 1e-5;
    opts.relative_tolerance = 1e-5;
    opts.adaptive_rho = true;

    auto sol = qp::solve_qp(qp, opts);
    E2E_ASSERT(sol.status == qp::QpStatus::optimal, "Portfolio solve optimal");
    E2E_ASSERT_NEAR(sol.x[0] + sol.x[1], 1.0, 1e-3, "Budget fully invested");
    E2E_ASSERT(0.10 * sol.x[0] + 0.15 * sol.x[1] >= 0.119, "Target return achieved");

    auto rep = qp::verify_qp_solution(qp, sol, 1e-4);
    E2E_ASSERT(rep.passed, "Portfolio KKT verified");
}

E2E_TEST(T4_APP_03_MultiPeriodSupplyChainLogistics,
         "Domain Scenario 3: Multi-period inventory flow with storage and production limits",
         Tier4, M1, 7) {
    // 3 Periods (t=1, 2, 3)
    // Production cost: $10/unit, Holding cost: $2/unit/period
    // Demand: d = [50, 120, 60]
    // Production capacity: 100 units/period
    // Max warehouse storage: 70 units
    // Initial inventory = 0
    // Variables: p1, p2, p3 (production), s1, s2, s3 (inventory end of period)
    // Flow balance:
    // p1 - s1 = 50
    // s1 + p2 - s2 = 120
    // s2 + p3 - s3 = 60
    // Objective: 10*(p1+p2+p3) + 2*(s1+s2+s3)
    model::Model model;
    model.name = "SUPPLY_CHAIN_3P";
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective = {10.0, 10.0, 10.0, 2.0, 2.0, 2.0}; // p1, p2, p3, s1, s2, s3

    model::SparseMatrixBuilder builder(3, 6);
    // Period 1: p1 - s1 = 50
    builder.add(0, 0, 1.0); builder.add(0, 3, -1.0);
    // Period 2: p2 + s1 - s2 = 120
    builder.add(1, 1, 1.0); builder.add(1, 3, 1.0); builder.add(1, 4, -1.0);
    // Period 3: p3 + s2 - s3 = 60
    builder.add(2, 2, 1.0); builder.add(2, 4, 1.0); builder.add(2, 5, -1.0);
    model.matrix = builder.build();

    model.row_lower = {model::Bound::finite(50.0), model::Bound::finite(120.0), model::Bound::finite(60.0)};
    model.row_upper = {model::Bound::finite(50.0), model::Bound::finite(120.0), model::Bound::finite(60.0)};
    model.row_name = {"BAL1", "BAL2", "BAL3"};

    // Production bounds [0, 100], Storage bounds [0, 70]
    model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0), model::Bound::finite(0.0),
                            model::Bound::finite(0.0), model::Bound::finite(0.0), model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(100.0), model::Bound::finite(100.0), model::Bound::finite(100.0),
                            model::Bound::finite(70.0), model::Bound::finite(70.0), model::Bound::finite(70.0)};
    model.variable_type = std::vector<model::VariableType>(6, model::VariableType::continuous);
    model.variable_name = {"P1", "P2", "P3", "S1", "S2", "S3"};
    model.validate();

    auto res = api::solve_model(model);
    E2E_ASSERT(res.status == lp::reference::SolveStatus::optimal, "Supply chain optimal");
    E2E_ASSERT(res.verified, "Supply chain solution certified");

    // Total demand = 230. Since max prod in period 2 is 100, period 1 must over-produce by 20 (p1 = 70, s1 = 20, p2 = 100, p3 = 60)
    // Total cost = 10*(70+100+60) + 2*(20+0+0) = 2300 + 40 = 2340
    E2E_ASSERT_NEAR(res.objective, 2340.0, 1e-4, "Optimal supply chain cost = 2340.0");
    E2E_ASSERT_NEAR(res.primal[0], 70.0, 1e-4, "Period 1 production = 70");
    E2E_ASSERT_NEAR(res.primal[3], 20.0, 1e-4, "Period 1 inventory = 20");
}

int main(int argc, char** argv) {
    return TestRegistry::instance().run(argc, argv);
}
