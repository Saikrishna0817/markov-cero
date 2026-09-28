#include "parallel_tree_search_test_internal.hpp"
#include <random>
#include <stdexcept>
namespace test_parallel_tree_search_test {
using namespace detail_parallel_tree_search_test;
namespace detail_parallel_tree_search_test {
void test_persistent_node_bounds() {
    using markov_cero::milp::NodeBounds;
    using markov_cero::model::Bound;
    const std::vector<Bound> root_lower{Bound::finite(0), Bound::finite(0)};
    const std::vector<Bound> root_upper{Bound::finite(10), Bound::finite(10)};
    const NodeBounds root;
    const auto down = root.with_upper(0, Bound::finite(4));
    const auto up = root.with_lower(0, Bound::finite(5));
    const auto nested = down.with_lower(1, Bound::finite(3));

    std::vector<Bound> lower, upper;
    down.materialize(root_lower, root_upper, lower, upper);
    assert(lower[0].value == 0 && upper[0].value == 4);
    up.materialize(root_lower, root_upper, lower, upper);
    assert(lower[0].value == 5 && upper[0].value == 10);
    nested.materialize(root_lower, root_upper, lower, upper);
    assert(lower[0].value == 0 && upper[0].value == 4 && lower[1].value == 3);
    NodeBounds::MaterializationScratch scratch;
    nested.materialize(root_lower, root_upper, lower, upper, scratch);
    const auto retained_path_capacity = scratch.retained_capacity();
    down.materialize(root_lower, root_upper, lower, upper, scratch);
    assert(retained_path_capacity >= 2 &&
           scratch.retained_capacity() == retained_path_capacity);
    assert(root.delta_count() == 0 && down.delta_count() == 1 && nested.delta_count() == 2);
    bool rejected_alias = false;
    auto alias_root_upper = root_upper;
    try { down.materialize(root_lower, alias_root_upper, lower, alias_root_upper); }
    catch (const std::invalid_argument&) { rejected_alias = true; }
    assert(rejected_alias && alias_root_upper[0].value == 10);

    markov_cero::milp::NodeCuts parent_cuts;
    markov_cero::milp::Cut inherited{{1.0}, 0.5, 1.0};
    parent_cuts.append({inherited});
    auto left_cuts = parent_cuts;
    auto right_cuts = parent_cuts;
    left_cuts.append({markov_cero::milp::Cut{{2.0}, 1.0, 2.0}});
    assert(parent_cuts.size() == 1 && right_cuts.size() == 1 && left_cuts.size() == 2);
    left_cuts.truncate(1);
    assert(parent_cuts.size() == 1 && left_cuts.values()[0].coefficients[0] == 1.0);

    markov_cero::milp::ThreadSafeNodeQueue queue;
    markov_cero::milp::BranchNode parent_node;
    parent_node.id = 41;
    std::optional<markov_cero::lp::dual::BasisState> basis;
    basis.emplace();
    basis->rows = 2;
    basis->columns = 3;
    basis->basic_variables = {0, 2};
    std::atomic<std::size_t> next_id{42};
    queue.push_branch_children(parent_node, 0, 0.5, 0.0,
        {Bound::finite(0)}, {Bound::finite(1)}, basis, next_id);
    bool became_active = false;
    const auto children = queue.pop_batch(false, 1.0, became_active);
    assert(children.size() == 2 && became_active);
    assert(children[0]->warm_basis && children[1]->warm_basis);
    assert(children[0]->warm_basis == children[1]->warm_basis);
    assert(children[0]->warm_basis->basic_variables == basis->basic_variables);
    queue.deactivate_worker();

    struct State { NodeBounds bounds; std::vector<Bound> lower, upper; };
    std::vector<Bound> large_lower(64, Bound::finite(0));
    std::vector<Bound> large_upper(64, Bound::finite(100));
    std::vector<State> states{{root, large_lower, large_upper}};
    std::mt19937 random(26119);
    for (int step = 0; step < 256; ++step) {
        const auto& parent = states[random() % states.size()];
        const std::size_t variable = random() % large_lower.size();
        const auto value = Bound::finite(static_cast<double>(random() % 101));
        State child{parent.bounds, parent.lower, parent.upper};
        if (step % 2 == 0) {
            child.bounds = child.bounds.with_lower(variable, value);
            child.lower[variable] = value;
        } else {
            child.bounds = child.bounds.with_upper(variable, value);
            child.upper[variable] = value;
        }
        child.bounds.materialize(large_lower, large_upper, lower, upper);
        for (std::size_t j = 0; j < large_lower.size(); ++j) {
            assert(lower[j].kind == child.lower[j].kind && lower[j].value == child.lower[j].value);
            assert(upper[j].kind == child.upper[j].kind && upper[j].value == child.upper[j].value);
        }
        states.push_back(std::move(child));
    }
}
}

namespace detail_parallel_tree_search_test {
void test_thread_safety_repeated_runs() {
    const auto knapsack = build_knapsack_model();
    const auto refinery = build_refinery_dispatch_model();

    markov_cero::milp::ParallelOptions opt;
    opt.num_threads = 4;

    // Run 20 iterations back-to-back on knapsack
    for (std::size_t i = 0; i < 20; ++i) {
        const auto res = markov_cero::milp::solve_parallel(knapsack, opt);
        assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
        assert(std::abs(res.objective - (-24.0)) < 1e-5);

        markov_cero::verify::Candidate candidate{res.primal, res.objective};
        const auto report = markov_cero::verify::verify_primal(knapsack, candidate);
        assert(report.passed);
        assert(report.maximum_integrality_violation < 1e-6);
    }

    // Run 20 iterations back-to-back on refinery dispatch
    for (std::size_t i = 0; i < 20; ++i) {
        const auto res = markov_cero::milp::solve_parallel(refinery, opt);
        assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
        assert(std::abs(res.objective - 120.0) < 1e-5);

        markov_cero::verify::Candidate candidate{res.primal, res.objective};
        const auto report = markov_cero::verify::verify_primal(refinery, candidate);
        assert(report.passed);
        assert(report.maximum_integrality_violation < 1e-6);
    }

    std::cout << "[+] test_thread_safety_repeated_runs passed (40 stress runs completed without "
                 "race or deadlock)\n";
}
}

namespace detail_parallel_tree_search_test {
void test_infeasible_parallel() {
    const auto model = build_infeasible_milp_model();

    for (std::size_t th : {1, 2, 4}) {
        markov_cero::milp::ParallelOptions opt;
        opt.num_threads = th;
        const auto res = markov_cero::milp::solve_parallel(model, opt);
        assert(res.status == markov_cero::lp::reference::SolveStatus::infeasible);
    }

    std::cout << "[+] test_infeasible_parallel passed\n";
}
}

namespace detail_parallel_tree_search_test {
void test_incumbent_manager_unit() {
    markov_cero::milp::IncumbentManager mgr(100.0);
    assert(mgr.has_incumbent());
    assert(std::abs(mgr.get_objective() - 100.0) < 1e-9);

    // Try worsening update: should fail
    bool up1 = mgr.update_if_better(120.0, {1.0, 2.0});
    assert(!up1);
    assert(std::abs(mgr.get_objective() - 100.0) < 1e-9);

    // Try improving update: should succeed
    bool up2 = mgr.update_if_better(80.0, {3.0, 4.0});
    assert(up2);
    assert(std::abs(mgr.get_objective() - 80.0) < 1e-9);
    const auto p = mgr.get_primal();
    assert(p.size() == 2);
    assert(p[0] == 3.0 && p[1] == 4.0);

    // Concurrent updates test
    std::vector<std::jthread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&mgr, i]() {
            double obj = 70.0 - static_cast<double>(i);
            mgr.update_if_better(obj, {obj, obj * 2.0});
        });
    }
    threads.clear();

    assert(mgr.get_objective() <= 63.0 + 1e-9);

    std::cout << "[+] test_incumbent_manager_unit passed\n";
}
}

}
