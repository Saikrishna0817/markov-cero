#pragma once

#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/milp/branch_node.hpp"
#include "markov_cero/model/model.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace markov_cero::milp {

/// Thread-safe min-heap priority queue of BranchNodes prioritizing lowest lower bound.
///
/// RW-2 rework notes:
/// - pop_batch() hands a worker up to N best-bounded nodes per lock acquisition,
///   amortizing contention over a subtree slice instead of one mutex round-trip
///   per node.
/// - Pruning at pop time is lazy: stale nodes are skipped while popping and the
///   heap is compacted only after a discard threshold, removing the per-pop
///   O(n) remove_if + make_heap that serialized all workers on the old design.
/// - Batch order is interleaved (best, worst, second-best, ...) so concurrent
///   workers pulling from one batch explore different subtree regions first,
///   which softens the work-stealing imbalance without per-worker deques.
class ThreadSafeNodeQueue {
  public:
    ThreadSafeNodeQueue() = default;
    ~ThreadSafeNodeQueue() = default;

    ThreadSafeNodeQueue(const ThreadSafeNodeQueue&) = delete;
    ThreadSafeNodeQueue& operator=(const ThreadSafeNodeQueue&) = delete;
    ThreadSafeNodeQueue(ThreadSafeNodeQueue&&) = delete;
    ThreadSafeNodeQueue& operator=(ThreadSafeNodeQueue&&) = delete;

    /// R5: select the node-selection policy used for every heap operation.
    /// Must be set before workers start pushing/popping (the parallel driver
    /// sets it once, from `ParallelOptions::node_selection`).
    void set_node_selection(NodeSelection policy) noexcept { comparator_.policy = policy; }
    [[nodiscard]] NodeSelection node_selection() const noexcept {
        return comparator_.policy;
    }

    void push(std::shared_ptr<BranchNode> node);
    void push_children(std::shared_ptr<BranchNode> left, std::shared_ptr<BranchNode> right);

    void push_branch_children(const BranchNode& parent, std::size_t branch_var, double branch_val,
                              double lower_bound,
                              const std::optional<lp::dual::BasisState>& warm_basis,
                              std::atomic<std::size_t>& next_node_id);

    /// Pop up to max_batch best-bounded nodes above the prune cutoff in one lock
    /// acquisition. Returns an empty vector only when the queue is drained-and-
    /// quiescent or stopped (the worker should then exit).
    [[nodiscard]] std::vector<std::shared_ptr<BranchNode>>
    pop_batch(bool was_active, double prune_cutoff, bool& became_active,
              std::size_t max_batch = 16);

    void deactivate_worker();
    void prune(double cutoff);
    void request_stop();
    [[nodiscard]] bool is_stopped() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t active_workers() const;
    [[nodiscard]] double min_lower_bound() const;
    void notify_all();

  private:
    void prune_locked(double cutoff);

    mutable    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::shared_ptr<BranchNode>> heap_;
    NodeComparator comparator_{};
    std::size_t active_workers_{0};
    bool stopped_{false};
    std::size_t prune_lazily_discarded_{0};
    std::atomic<bool> need_notify_{false};
};

using WorkQueue = ThreadSafeNodeQueue;

} // namespace markov_cero::milp
