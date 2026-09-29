#include "milp_test_internal.hpp"
#include "markov_cero/milp/parallel_tree_search.hpp"
namespace test_milp_test {
using namespace detail_milp_test;
void test_node_selection_policies() {
    // 7-item knapsack where exploration order matters: max 17x1 + 7x2 + 11x3 +
    // 11x4 + 29x5 + 21x6 + 22x7 s.t. 8x1 + 15x2 + 16x3 + 5x4 + 14x5 + 15x6 +
    // 15x7 <= 36.96, x binary. (The earlier 5-item instance exhausted its
    // tree in 5 nodes under every policy, so it could not distinguish them;
    // this one explores 13 / 17 / 13 nodes for best_bound / depth_first /
    // best_bound_dive.)
    markov_cero::model::Model model;
    model.name = "NODE_SELECTION";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-17.0, -7.0, -11.0, -11.0, -29.0, -21.0, -22.0};
    model.objective_offset = 0.0;

    const std::size_t n = 7;
    const double weights[n] = {8.0, 15.0, 16.0, 5.0, 14.0, 15.0, 15.0};
    markov_cero::model::SparseMatrixBuilder builder(1, n);
    for (std::size_t j = 0; j < n; ++j) {
        builder.add(0, j, weights[j]);
    }
    model.matrix = builder.build();

    model.row_lower = {markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::finite(36.96)};
    model.row_name = {"CAPACITY"};
    model.variable_lower.assign(n, markov_cero::model::Bound::finite(0.0));
    model.variable_upper.assign(n, markov_cero::model::Bound::finite(1.0));
    model.variable_type.assign(n, markov_cero::model::VariableType::binary);
    model.variable_name = {"X1", "X2", "X3", "X4", "X5", "X6", "X7"};
    model.validate();

    const markov_cero::milp::NodeSelection policies[] = {
        markov_cero::milp::NodeSelection::best_bound,
        markov_cero::milp::NodeSelection::depth_first,
        markov_cero::milp::NodeSelection::best_bound_dive,
    };

    double reference_objective = 0.0;
    std::size_t first_nodes = 0;
    bool distinct = false;
    for (std::size_t p = 0; p < 3; ++p) {
        markov_cero::milp::Options options;
        options.node_selection = policies[p];
        const auto result = markov_cero::milp::solve(model, options);
        assert(result.status == markov_cero::lp::reference::SolveStatus::optimal);
        if (p == 0) {
            reference_objective = result.objective;
            first_nodes = result.nodes_explored;
        } else {
            assert(std::abs(result.objective - reference_objective) < 1e-5);
            if (result.nodes_explored != first_nodes) {
                distinct = true;
            }
        }
        // The incumbent must still verify against the original model.
        markov_cero::verify::Candidate candidate{result.primal, result.objective};
        const auto report = markov_cero::verify::verify_primal(model, candidate);
        assert(report.passed);
    }
    // Distinct search orders: at least one policy differs from best-bound in
    // node count on this instance (guards against all policies degenerating to
    // the same comparator).
    assert(distinct);
    std::cout << "[+] test_node_selection_policies passed: obj=" << reference_objective
              << ", best_bound_nodes=" << first_nodes << "\n";
}
void test_pseudo_cost_branching() {
    markov_cero::model::Model model;
    model.name = "PSEUDO_COST_TEST";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-5.0, -7.0, -10.0, -3.0, -1.0};
    model.objective_offset = 0.0;

    markov_cero::model::SparseMatrixBuilder builder(2, 5);
    builder.add(0, 0, 1.0); builder.add(0, 1, 3.0); builder.add(0, 2, 5.0); builder.add(0, 3, 2.0); builder.add(0, 4, 1.0);
    builder.add(1, 0, 2.0); builder.add(1, 1, 1.0); builder.add(1, 2, 3.0); builder.add(1, 3, 4.0); builder.add(1, 4, 2.0);
    model.matrix = builder.build();

    model.row_lower = {markov_cero::model::Bound::negative_infinity(), markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::finite(7.0), markov_cero::model::Bound::finite(6.0)};
    model.row_name = {"R1", "R2"};

    model.variable_lower = std::vector<markov_cero::model::Bound>(5, markov_cero::model::Bound::finite(0.0));
    model.variable_upper = std::vector<markov_cero::model::Bound>(5, markov_cero::model::Bound::finite(1.0));
    model.variable_type = std::vector<markov_cero::model::VariableType>(5, markov_cero::model::VariableType::binary);
    model.variable_name = {"X1", "X2", "X3", "X4", "X5"};
    model.validate();

    markov_cero::milp::Options options;
    options.branching_strategy = markov_cero::milp::BranchingStrategy::pseudo_cost;
    options.enable_strong_branching = false;
    const auto result = markov_cero::milp::solve(model, options);
    assert(result.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(result.nodes_explored >= 2);

    markov_cero::verify::Candidate candidate{result.primal, result.objective};
    const auto report = markov_cero::verify::verify_primal(model, candidate);
    assert(report.passed);
    std::cout << "[+] test_pseudo_cost_branching passed: nodes=" << result.nodes_explored << " obj=" << result.objective << "\n";
}
void test_near_integer_root_rejected() {
    using namespace markov_cero;
    model::Model m;
    m.objective = {0};
    model::SparseMatrixBuilder a(1, 1);
    a.add(0, 0, 1);
    m.matrix = a.build();
    m.row_lower = {model::Bound::finite(1e-5)};
    m.row_upper = m.row_lower;
    m.row_name = {"EQ"};
    m.variable_lower = {model::Bound::finite(0)};
    m.variable_upper = {model::Bound::finite(1)};
    m.variable_type = {model::VariableType::integer};
    m.variable_name = {"X"};
    m.validate();
    milp::Options options;
    options.feasibility_tolerance = 1e-7;
    options.integrality_tolerance = 1e-4;
    assert(milp::solve(m, options).status != lp::reference::SolveStatus::optimal);
    milp::ParallelOptions parallel;
    parallel.feasibility_tolerance = 1e-7;
    parallel.integrality_tolerance = 1e-4;
    parallel.num_threads = 1;
    assert(milp::solve_parallel(m, parallel).status != lp::reference::SolveStatus::optimal);
}
void test_huge_parallel_incumbent() {
    using namespace markov_cero;
    model::Model m;
    m.objective = {0};
    m.objective_offset = 9e307;
    model::SparseMatrixBuilder a(1, 1);
    a.add(0, 0, 1);
    m.matrix = a.build();
    m.row_lower = {model::Bound::finite(0.5)};
    m.row_upper = {model::Bound::positive_infinity()};
    m.row_name = {"LOWER"};
    m.variable_lower = {model::Bound::finite(0)};
    m.variable_upper = {model::Bound::finite(1)};
    m.variable_type = {model::VariableType::binary};
    m.variable_name = {"X"};
    m.validate();
    milp::ParallelOptions options;
    options.num_threads = 1;
    options.enable_cuts = false;
    options.enable_strong_branching = false;
    options.enable_heuristics = true;
    const auto result = milp::solve_parallel(m, options);
    assert(result.status == lp::reference::SolveStatus::optimal);
    assert(result.primal.size() == 1 && result.primal[0] == 1);
    assert(result.objective == 9e307);
}
}
