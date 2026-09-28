#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

namespace {

void test_knapsack_01() {
    // max 10 x1 + 14 x2 + 12 x3
    // min -10 x1 - 14 x2 - 12 x3
    // s.t. 4 x1 + 6 x2 + 5 x3 <= 10
    // x1, x2, x3 in {0, 1}
    //
    // Continuous LP relaxation: x1=1, x3=1, x2=1/6, obj = -24.3333
    // Integer optimum: x1=1, x2=1, x3=0, obj = -24.0

    markov_cero::model::Model model;
    model.name = "KNAPSACK";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-10.0, -14.0, -12.0};
    model.objective_offset = 0.0;

    markov_cero::model::SparseMatrixBuilder builder(1, 3);
    builder.add(0, 0, 4.0);
    builder.add(0, 1, 6.0);
    builder.add(0, 2, 5.0);
    model.matrix = builder.build();

    model.row_lower = {markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::finite(10.0)};
    model.row_name = {"CAPACITY"};

    model.variable_lower = {markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0)};
    model.variable_type = {markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary};
    model.variable_name = {"X1", "X2", "X3"};
    model.validate();

    markov_cero::milp::Options options;
    const auto result = markov_cero::milp::solve(model, options);

    assert(result.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(result.objective - (-24.0)) < 1e-5);
    assert(result.primal.size() == 3);
    assert(std::abs(result.primal[0] - 1.0) < 1e-5);
    assert(std::abs(result.primal[1] - 1.0) < 1e-5);
    assert(std::abs(result.primal[2] - 0.0) < 1e-5);

    // Verify independently with zero-trust primal verifier
    markov_cero::verify::Candidate candidate{result.primal, result.objective};
    const auto report = markov_cero::verify::verify_primal(model, candidate);
    assert(report.passed);
    assert(report.maximum_integrality_violation < 1e-6);

    auto expired_options = options;
    expired_options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto expired = markov_cero::milp::solve(model, expired_options);
    assert(expired.status == markov_cero::lp::reference::SolveStatus::resource_limit);
    assert(expired.message.find("deadline") != std::string::npos ||
           expired.message.find("ResourceLimit") != std::string::npos);

    auto capped_options = options;
    capped_options.max_nodes = 1;
    capped_options.enable_heuristics = false;
    capped_options.enable_cuts = false;
    capped_options.enable_strong_branching = false;
    const auto capped = markov_cero::milp::solve(model, capped_options);
    assert(capped.status == markov_cero::lp::reference::SolveStatus::resource_limit);
    std::cout << "[+] test_knapsack_01 passed\n";
}

void test_sparse_pdlp_root_relaxation_above_dense_row_limit() {
    // Exercise the sparse node-LP fallback with a model that would exceed the
    // dense tableau row cap while remaining trivially certifiable.
    constexpr std::size_t rows = 4097;
    markov_cero::model::Model model;
    model.name = "SPARSE_ROOT_ABOVE_DENSE_LIMIT";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {0.0};
    markov_cero::model::SparseMatrixBuilder builder(rows, 1);
    model.matrix = builder.build();
    model.row_lower.assign(rows, markov_cero::model::Bound::finite(0.0));
    model.row_upper.assign(rows, markov_cero::model::Bound::finite(0.0));
    model.row_name.resize(rows);
    for (std::size_t i = 0; i < rows; ++i)
        model.row_name[i] = "R" + std::to_string(i);
    model.variable_lower = {markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::finite(1.0)};
    model.variable_type = {markov_cero::model::VariableType::binary};
    model.variable_name = {"X"};
    model.validate();

    markov_cero::milp::Options options;
    options.enable_cuts = false;
    options.enable_strong_branching = false;
    const auto result = markov_cero::milp::solve(model, options);
    assert(result.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(result.best_bound <= result.objective + 1e-8);
    assert(result.relative_gap <= options.relative_gap_tolerance);
    const auto report = markov_cero::verify::verify_primal(
        model, {result.primal, result.objective});
    assert(report.passed);
    std::cout << "[+] test_sparse_pdlp_root_relaxation_above_dense_row_limit passed\n";
}

void test_maximization_objective_and_bound_signs() {
    markov_cero::model::Model model;
    model.name = "MAXIMIZATION_SIGN_TEST";
    model.objective_sense = markov_cero::model::ObjectiveSense::maximize;
    model.objective_offset = 11.0;
    model.objective = {3.0, 2.0};
    markov_cero::model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 2.0);
    builder.add(0, 1, 1.0);
    model.matrix = builder.build();
    model.row_lower = {markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::finite(4.0)};
    model.row_name = {"CAP"};
    model.variable_lower = {markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::finite(10.0),
                            markov_cero::model::Bound::finite(10.0)};
    model.variable_type = {markov_cero::model::VariableType::integer,
                           markov_cero::model::VariableType::integer};
    model.variable_name = {"X", "Y"};

    const auto result = markov_cero::milp::solve(model);
    assert(result.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(result.objective - 19.0) < 1e-6); // 2*4 + offset 11
    assert(std::abs(result.best_bound - 19.0) < 1e-6); // maximization upper bound
    const auto report = markov_cero::verify::verify_primal(
        model, {result.primal, result.objective});
    assert(report.passed);
}

void test_refinery_discrete_dispatch() {
    // MRPL Scenario: crude tanker discrete batches
    // min 50 x1 + 40 x2
    // s.t. x1 + x2 >= 3 (total batches needed >= 3)
    //      3 x1 + 2 x2 <= 8 (berth pipeline hours <= 8)
    // x1, x2 in {0, 1, 2, 3, ...}
    //
    // Valid integer points:
    // (0, 3): 3*0 + 2*3 = 6 <= 8, x1+x2 = 3 >= 3, cost = 50*0 + 40*3 = 120
    // (1, 2): 3*1 + 2*2 = 7 <= 8, x1+x2 = 3 >= 3, cost = 50*1 + 40*2 = 130
    // (2, 1): 3*2 + 2*1 = 8 <= 8, x1+x2 = 3 >= 3, cost = 50*2 + 40*1 = 140
    // Optimum is (0, 3) with cost 120

    markov_cero::model::Model model;
    model.name = "REFINERY_DISPATCH";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {50.0, 40.0};
    model.objective_offset = 0.0;

    markov_cero::model::SparseMatrixBuilder builder(2, 2);
    // Row 0: x1 + x2 >= 3
    builder.add(0, 0, 1.0);
    builder.add(0, 1, 1.0);
    // Row 1: 3 x1 + 2 x2 <= 8
    builder.add(1, 0, 3.0);
    builder.add(1, 1, 2.0);
    model.matrix = builder.build();

    model.row_lower = {markov_cero::model::Bound::finite(3.0),
                       markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::positive_infinity(),
                       markov_cero::model::Bound::finite(8.0)};
    model.row_name = {"MIN_BATCHES", "MAX_PIPELINE_HOURS"};

    model.variable_lower = {markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::finite(5.0),
                            markov_cero::model::Bound::finite(5.0)};
    model.variable_type = {markov_cero::model::VariableType::integer,
                           markov_cero::model::VariableType::integer};
    model.variable_name = {"TANKER_A", "TANKER_B"};
    model.validate();

    markov_cero::milp::Options options;
    const auto result = markov_cero::milp::solve(model, options);

    assert(result.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(result.objective - 120.0) < 1e-5);
    assert(std::abs(result.primal[0] - 0.0) < 1e-5);
    assert(std::abs(result.primal[1] - 3.0) < 1e-5);

    markov_cero::verify::Candidate candidate{result.primal, result.objective};
    const auto report = markov_cero::verify::verify_primal(model, candidate);
    assert(report.passed);
    std::cout << "[+] test_refinery_discrete_dispatch passed\n";
}

void test_infeasible_milp() {
    // x1 + x2 <= 1
    // x1 + x2 >= 2
    // x1, x2 in {0, 1}
    markov_cero::model::Model model;
    model.name = "INFEASIBLE_MIP";
    model.objective = {1.0, 1.0};

    markov_cero::model::SparseMatrixBuilder builder(2, 2);
    builder.add(0, 0, 1.0);
    builder.add(0, 1, 1.0);
    builder.add(1, 0, 1.0);
    builder.add(1, 1, 1.0);
    model.matrix = builder.build();

    model.row_lower = {markov_cero::model::Bound::negative_infinity(),
                       markov_cero::model::Bound::finite(2.0)};
    model.row_upper = {markov_cero::model::Bound::finite(1.0),
                       markov_cero::model::Bound::positive_infinity()};
    model.row_name = {"R1", "R2"};

    model.variable_lower = {markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0)};
    model.variable_type = {markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary};
    model.variable_name = {"X1", "X2"};
    model.validate();

    markov_cero::milp::Options options;
    const auto result = markov_cero::milp::solve(model, options);
    assert(result.status == markov_cero::lp::reference::SolveStatus::infeasible);
    std::cout << "[+] test_infeasible_milp passed\n";
}

} // namespace

// R5: node selection. Every policy must reach the same certified optimum, and
// the policies must be genuinely distinct (node counts differ on at least one
// instance) rather than aliases of best-bound.
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

int main() {
    try {
        test_knapsack_01();
        test_sparse_pdlp_root_relaxation_above_dense_row_limit();
        test_maximization_objective_and_bound_signs();
        test_refinery_discrete_dispatch();
        test_infeasible_milp();
        test_node_selection_policies();
        test_pseudo_cost_branching();
        std::cout << "All MILP unit tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << "\n";
        return 1;
    }
}
