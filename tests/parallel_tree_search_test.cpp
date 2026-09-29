#include "parallel_tree_search_test_internal.hpp"
#include "../src/milp/parallel_tree_search_internal.hpp"
namespace test_parallel_tree_search_test {
using namespace detail_parallel_tree_search_test;
namespace detail_parallel_tree_search_test {
markov_cero::model::Model build_knapsack_model() {
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
    return model;
}
}

namespace detail_parallel_tree_search_test {
markov_cero::model::Model build_refinery_dispatch_model() {
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
    builder.add(0, 0, 1.0);
    builder.add(0, 1, 1.0);
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
    return model;
}
}

namespace detail_parallel_tree_search_test {
markov_cero::model::Model build_infeasible_milp_model() {
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
    return model;
}
}

namespace detail_parallel_tree_search_test {
void test_knapsack_multi_threads() {
    const auto model = build_knapsack_model();

    // 1-thread
    markov_cero::milp::ParallelOptions opt1;
    opt1.num_threads = 1;
    const auto res1 = markov_cero::milp::solve_parallel(model, opt1);
    assert(res1.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res1.objective - (-24.0)) < 1e-5);

    // 2-threads
    markov_cero::milp::ParallelOptions opt2;
    opt2.num_threads = 2;
    const auto res2 = markov_cero::milp::solve_parallel(model, opt2);
    assert(res2.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res2.objective - (-24.0)) < 1e-5);

    // 4-threads
    markov_cero::milp::ParallelOptions opt4;
    opt4.num_threads = 4;
    const auto res4 = markov_cero::milp::solve_parallel(model, opt4);
    assert(res4.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res4.objective - (-24.0)) < 1e-5);

    // Confirm identical optimal integer objective across threads
    assert(std::abs(res1.objective - res2.objective) < 1e-6);
    assert(std::abs(res1.objective - res4.objective) < 1e-6);

    // Verify primal feasibility and integrality
    for (const auto& res : {res1, res2, res4}) {
        markov_cero::verify::Candidate candidate{res.primal, res.objective};
        const auto report = markov_cero::verify::verify_primal(model, candidate);
        assert(report.passed);
        assert(report.maximum_integrality_violation < 1e-6);
        assert(std::abs(res.primal[0] - 1.0) < 1e-5);
        assert(std::abs(res.primal[1] - 1.0) < 1e-5);
        assert(std::abs(res.primal[2] - 0.0) < 1e-5);
    }

    auto expired_options = opt2;
    expired_options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto expired = markov_cero::milp::solve_parallel(model, expired_options);
    assert(expired.status == markov_cero::lp::reference::SolveStatus::resource_limit);
    assert(expired.status != markov_cero::lp::reference::SolveStatus::optimal);

    auto capped_options = opt4;
    capped_options.max_nodes = 1;
    capped_options.enable_heuristics = false;
    capped_options.enable_cuts = false;
    capped_options.enable_strong_branching = false;
    const auto capped = markov_cero::milp::solve_parallel(model, capped_options);
    assert(capped.status == markov_cero::lp::reference::SolveStatus::resource_limit);

    std::cout << "[+] test_knapsack_multi_threads passed (1, 2, 4 threads verified)\n";
}
}

namespace detail_parallel_tree_search_test {
void test_refinery_dispatch_multi_threads() {
    const auto model = build_refinery_dispatch_model();

    // 1-thread
    markov_cero::milp::ParallelOptions opt1;
    opt1.num_threads = 1;
    const auto res1 = markov_cero::milp::solve_parallel(model, opt1);
    assert(res1.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res1.objective - 120.0) < 1e-5);

    // 2-threads
    markov_cero::milp::ParallelOptions opt2;
    opt2.num_threads = 2;
    const auto res2 = markov_cero::milp::solve_parallel(model, opt2);
    assert(res2.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res2.objective - 120.0) < 1e-5);

    // 4-threads
    markov_cero::milp::ParallelOptions opt4;
    opt4.num_threads = 4;
    const auto res4 = markov_cero::milp::solve_parallel(model, opt4);
    assert(res4.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res4.objective - 120.0) < 1e-5);

    // Confirm identical optimal integer objective across threads
    assert(std::abs(res1.objective - res2.objective) < 1e-6);
    assert(std::abs(res1.objective - res4.objective) < 1e-6);

    // Verify primal feasibility and integrality
    for (const auto& res : {res1, res2, res4}) {
        markov_cero::verify::Candidate candidate{res.primal, res.objective};
        const auto report = markov_cero::verify::verify_primal(model, candidate);
        assert(report.passed);
        assert(report.maximum_integrality_violation < 1e-6);
        assert(std::abs(res.primal[0] - 0.0) < 1e-5);
        assert(std::abs(res.primal[1] - 3.0) < 1e-5);
    }

    std::cout << "[+] test_refinery_dispatch_multi_threads passed (1, 2, 4 threads verified)\n";
}
}

}
using namespace test_parallel_tree_search_test;
using namespace test_parallel_tree_search_test::detail_parallel_tree_search_test;
void test_parallel_cut_helper_worker_isolation() {
    using namespace markov_cero;
    const auto model = build_knapsack_model();
    milp::ParallelOptions options;
    options.enable_heuristics = false;
    std::atomic<std::size_t> iterations{0}, cuts{0};
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    for (std::size_t worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&, worker] {
            milp::BranchNode node;
            node.id = worker + 1;
            const auto result = milp::detail_parallel_tree_search::solve_parallel_node_with_cuts(
                node, model, options, std::nullopt, model.variable_lower,
                model.variable_upper, worker + 1, iterations, cuts);
            if (result.status != lp::reference::SolveStatus::optimal ||
                result.primal.size() != model.matrix.column_count) failed.store(true);
        });
    }
    for (auto& worker : workers) worker.join();
    if (failed || iterations.load() == 0)
        throw std::runtime_error("worker-local cut helper failed parallel solve");
}
int main() {
    try {
        test_persistent_node_bounds();
        test_thread_safe_queue_unit();
        test_queue_capacity_and_solver_status();
        test_queue_lazy_prune_batch();
        test_queue_batch_interleave_order();
        test_incumbent_manager_unit();
        test_knapsack_multi_threads();
        test_refinery_dispatch_multi_threads();
        test_thread_safety_repeated_runs();
        test_parallel_cut_helper_worker_isolation();
        test_infeasible_parallel();
        std::cout << "\n=======================================================\n";
        std::cout << "All Parallel Tree Search Tests PASSED Successfully!\n";
        std::cout << "=======================================================\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << "\n";
        return 1;
    }
}
