#include "parallel_tree_search_test_internal.hpp"

namespace test_parallel_tree_search_test {
using namespace detail_parallel_tree_search_test;
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
void test_queue_capacity_and_solver_status() {
    using markov_cero::milp::BranchNode;
    markov_cero::milp::ThreadSafeNodeQueue queue;
    queue.set_maximum_size(1);
    auto left = std::make_shared<BranchNode>();
    auto right = std::make_shared<BranchNode>();
    left->lower_bound = 7.0;
    right->lower_bound = 3.0;
    assert(!queue.push_children(left, right));
    assert(queue.size() == 0 && queue.capacity_exhausted());
    assert(std::abs(queue.min_lower_bound() - 3.0) < 1e-12);

    auto options = markov_cero::milp::ParallelOptions{};
    options.num_threads = 2;
    options.max_queued_nodes = 1;
    options.enable_heuristics = false;
    options.enable_cuts = false;
    options.enable_strong_branching = false;
    const auto result = markov_cero::milp::solve_parallel(build_knapsack_model(), options);
    assert(result.status == markov_cero::lp::reference::SolveStatus::resource_limit);
    assert(result.message.find("queued-node capacity") != std::string::npos);
    assert(result.best_bound <= -24.333333 + 1e-5);
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
    // Cutoff 9.5 (strictly between the guarded bounds of 9 and 10; §3.1
    // prune_guard) discards bounds 10..19 lazily and returns the 10 best.
    auto batch = queue.pop_batch(false, 9.5, became_active, 16);
    assert(batch.size() == 10);
    assert(batch.front()->lower_bound <= batch.back()->lower_bound + 1e-12);
    for (const auto& n : batch) {
        assert(n->lower_bound < 10.0);
    }

    // Next batch drains the remainder (empty -> only when quiescent or stopped;
    // here one worker was activated then deactivated by the next call chain).
    queue.deactivate_worker();
    auto empty_batch = queue.pop_batch(false, 9.5, became_active);
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

}
