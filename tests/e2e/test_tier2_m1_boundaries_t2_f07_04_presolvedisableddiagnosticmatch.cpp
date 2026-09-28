#include "test_tier2_m1_boundaries_fixtures.hpp"
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
