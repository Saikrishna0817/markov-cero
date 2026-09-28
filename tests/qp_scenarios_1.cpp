#include "qp_test_fixtures.hpp"
#include "markov_cero/milp/node_lp.hpp"
void qp_scenario_7() {
        model::Model miqp;
        miqp.name = "MIQP_TOY";
        miqp.objective_sense = model::ObjectiveSense::minimize;
        miqp.objective_offset = 9.28;
        miqp.variable_name = {"x1", "x2"};
        miqp.objective = {-2.4, -5.6};
        miqp.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
        miqp.variable_upper = {model::Bound::finite(3.0), model::Bound::finite(3.0)};
        miqp.variable_type = {model::VariableType::integer, model::VariableType::integer};

        miqp.row_name = {"c1"};
        miqp.row_lower = {model::Bound::negative_infinity()};
        miqp.row_upper = {model::Bound::finite(3.0)};

        model::SparseMatrixBuilder ab(1, 2);
        ab.add(0, 0, 1.0);
        ab.add(0, 1, 1.0);
        miqp.matrix = ab.build();

        model::SparseMatrixBuilder qb(2, 2);
        qb.add(0, 0, 2.0);
        qb.add(1, 1, 2.0);
        miqp.has_quadratic_objective = true;
        miqp.quadratic_matrix = qb.build();
        miqp.validate();

        milp::Options opts;
        opts.max_iterations = 2000;
        opts.feasibility_tolerance = 1e-4;
        opts.integrality_tolerance = 1e-4;
        const auto res = milp::solve(miqp, opts);
        require(res.status == lp::reference::SolveStatus::optimal,
                "MIQP solved to optimality");
        require(std::round(res.primal[0]) == 1.0, "x1 is 1");
        require(std::round(res.primal[1]) == 2.0, "x2 is 2");
        require(std::abs(res.objective - 0.68) < 1e-2, "MIQP objective is ~0.68");
    }
void qp_scenario_8() {
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
        const auto qp = make_quadratic_model(model);
        QpOptions opts;
        opts.adaptive_rho = true;
        opts.adaptive_rho_interval = 25;
        opts.rho_init = 0.1;
        opts.max_iterations = 5000;
        opts.absolute_tolerance = 1e-4;
        opts.relative_tolerance = 1e-4;
        const auto sol = solve_qp(qp, opts);
        require(sol.status == QpStatus::optimal, "adaptive rho QP optimal");
        require(sol.refactorization_count > 0, "refactorization occurred during adaptive rho");
}

void qp_scenario_9() {
    model::Model source;
    source.name = "MIQP_NODE_BOUND_OVERLAY";
    source.objective = {-4.0};
    source.variable_name = {"x"};
    source.variable_lower = {model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(10.0)};
    source.variable_type = {model::VariableType::integer};
    source.matrix = model::SparseMatrixBuilder(0, 1).build();
    model::SparseMatrixBuilder quadratic(1, 1);
    quadratic.add(0, 0, 2.0);
    source.has_quadratic_objective = true;
    source.quadratic_matrix = quadratic.build();
    source.validate();

    const auto result = milp::solve_node_qp(
        source, milp::Options{}, {model::Bound::finite(3.0)},
        {model::Bound::finite(10.0)});
    require(result.status == lp::reference::SolveStatus::optimal,
            "MIQP node relaxation with tightened bound is optimal");
    require(result.primal.size() == 1 && std::abs(result.primal[0] - 3.0) < 1e-3,
            "MIQP node solution respects tightened lower bound");
    require(std::abs(result.objective + 3.0) < 1e-3,
            "MIQP node objective uses tightened bound");
    require(result.lower_bound <= result.objective + 1e-7 &&
                result.lower_bound > result.objective - 1e-3,
            "MIQP node supporting bound uses the tightened box");
}
