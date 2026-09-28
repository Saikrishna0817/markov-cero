#include "parallel_tree_search_test_internal.hpp"
namespace test_parallel_tree_search_test {
using namespace detail_parallel_tree_search_test;
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
void test_thread_safe_queue_unit() {
    markov_cero::milp::ThreadSafeNodeQueue queue;

    auto n1 = std::make_shared<markov_cero::milp::BranchNode>();
    n1->id = 1;
    n1->lower_bound = 10.0;

    auto n2 = std::make_shared<markov_cero::milp::BranchNode>();
    n2->id = 2;
    n2->lower_bound = 5.0;

    auto n3 = std::make_shared<markov_cero::milp::BranchNode>();
    n3->id = 3;
    n3->lower_bound = 20.0;

    queue.push(n1);
    queue.push(n2);
    queue.push(n3);

    assert(queue.size() == 3);
    assert(std::abs(queue.min_lower_bound() - 5.0) < 1e-9);

    // Prune nodes with lower_bound >= 15.0 (should prune n3)
    queue.prune(15.0);
    assert(queue.size() == 2);

    // RW-2: batch pop — one acquisition returns the best-bounded nodes.
    bool became_active = false;
    auto batch = queue.pop_batch(false, 100.0, became_active);
    assert(batch.size() == 2);
    assert(batch[0]->id == 2); // lowest lower bound first
    assert(batch[1]->id == 1);
    assert(became_active);

    queue.deactivate_worker();
    assert(queue.empty());

    std::cout << "[+] test_thread_safe_queue_unit passed (batch API)\n";
}
}

namespace detail_parallel_tree_search_test {
void test_queue_lazy_prune_batch() {
    // RW-2: stale nodes (below the cutoff) are filtered at pop time without a
    // global heap compaction on each prune request.
    markov_cero::milp::ThreadSafeNodeQueue queue;
    for (std::size_t i = 0; i < 20; ++i) {
        auto n = std::make_shared<markov_cero::milp::BranchNode>();
        n->id = i;
        n->lower_bound = static_cast<double>(i); // bounds 0..19
        queue.push(n);
    }

    bool became_active = false;
    // Cutoff 10.0 discards bounds 10..19 lazily and returns the 10 best.
    auto batch = queue.pop_batch(false, 10.0, became_active, 16);
    assert(batch.size() == 10);
    assert(batch.front()->lower_bound <= batch.back()->lower_bound + 1e-12);
    for (const auto& n : batch) {
        assert(n->lower_bound < 10.0);
    }

    // Next batch drains the remainder (empty -> only when quiescent or stopped;
    // here one worker was activated then deactivated by the next call chain).
    queue.deactivate_worker();
    auto empty_batch = queue.pop_batch(false, 10.0, became_active);
    assert(empty_batch.empty()); // heap only holds pruned nodes; quiescent => stopped

    std::cout << "[+] test_queue_lazy_prune_batch passed\n";
}
}

namespace detail_parallel_tree_search_test {
void test_queue_batch_interleave_order() {
    // RW-2: a batch is interleaved (best, worst, 2nd-best, ...) at the worker
    // level; the queue itself must hand out nodes in strict best-bound order so
    // interleaving preserves global best-first semantics.
    markov_cero::milp::ThreadSafeNodeQueue queue;
    const double bounds[] = {5.0, 1.0, 9.0, 3.0, 7.0};
    for (std::size_t i = 0; i < 5; ++i) {
        auto n = std::make_shared<markov_cero::milp::BranchNode>();
        n->id = 100 + i;
        n->lower_bound = bounds[i];
        queue.push(n);
    }

    bool became_active = false;
    auto batch = queue.pop_batch(false, 100.0, became_active, 4);
    assert(batch.size() == 4);
    // Expected best-bound order: 1, 3, 5, 7
    const double expect[] = {1.0, 3.0, 5.0, 7.0};
    for (std::size_t i = 0; i < 4; ++i) {
        assert(std::abs(batch[i]->lower_bound - expect[i]) < 1e-12);
    }
    queue.deactivate_worker();

    std::cout << "[+] test_queue_batch_interleave_order passed\n";
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
