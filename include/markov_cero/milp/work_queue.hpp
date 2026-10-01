#pragma once

#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/milp/branch_node.hpp"
#include "markov_cero/model/model.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <limits>
#include <mutex>
#include <optional>
#include <vector>

namespace markov_cero::milp {

/// MIP-01 contract §4: outcome of evaluating one branch partition at push time.
enum class ChildPushStatus {
    accepted,               ///< at least one child enqueued (or the queue
                            ///< stopped/capped exactly as push_children reports)
    split_rejected,         ///< floor(v) >= ceil(v): degenerate split (§2 P6)
    empty_integer_domain    ///< both gates reject: §4.2 conclusive emptiness
};

/// Maximum nodes handed out by one pop_batch call. Amortizes queue-lock
/// contention over a subtree slice instead of one mutex round-trip per node,
/// and removes the per-pop O(n) heap prune that serialized all workers (RW-2).
inline constexpr std::size_t kDefaultBatchSize = 16;

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

    void set_maximum_size(std::size_t maximum_size);

    bool push(std::shared_ptr<BranchNode> node);
    bool push_children(std::shared_ptr<BranchNode> left,
                       std::shared_ptr<BranchNode> right);

    /// Pop up to max_batch best-bounded nodes above the prune cutoff in one lock
    /// acquisition. Returns an empty vector only when the queue is drained-and-
    /// quiescent or stopped (the worker should then exit).
    [[nodiscard]] std::vector<std::shared_ptr<BranchNode>>
    pop_batch(bool was_active, double prune_cutoff, bool& became_active,
              std::size_t max_batch = kDefaultBatchSize);

    void deactivate_worker();
    void prune(double cutoff);
    void request_stop();
    /// MIP-01 contract §1 F3: fold the bound of a node removed from the live
    /// set without proof (unresolved relaxation, refused allocation, failed
    /// branch selection) into the frontier minimum, so the reported tree
    /// bound can never overstate progress. Mirrors the capacity-drop fold.
    void note_dropped_bound(double bound);
    /// MIP-01 contract §4.2: record a node whose split gates proved the integer
    /// domain empty (conclusive emptiness; never a bound claim, never silent).
    void note_empty_domain() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++empty_domain_nodes_;
    }
    [[nodiscard]] std::size_t empty_domain_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return empty_domain_nodes_;
    }
    [[nodiscard]] bool is_stopped() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool capacity_exhausted() const;
    [[nodiscard]] std::size_t active_workers() const;
    [[nodiscard]] double min_lower_bound() const;
    /// Peak live queue size since construction (W01/IR-19 evidence).
    [[nodiscard]] std::size_t peak_size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return peak_size_;
    }
    void notify_all();

  private:
    void prune_locked(double cutoff);

    mutable    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::shared_ptr<BranchNode>> heap_;
    NodeComparator comparator_{};
    std::size_t maximum_size_{std::numeric_limits<std::size_t>::max()};
    double minimum_dropped_bound_{std::numeric_limits<double>::infinity()};
    std::size_t active_workers_{0};
    bool stopped_{false};
    bool capacity_exhausted_{false};
    std::size_t prune_lazily_discarded_{0};
    std::size_t empty_domain_nodes_{0};
    std::size_t peak_size_{0};
    std::atomic<bool> need_notify_{false};
};

using WorkQueue = ThreadSafeNodeQueue;

/// MIP-01 contract §4: build the down/up children of one split. Degenerate
/// splits and empty integer domains are reported as statuses instead of
/// pushing, so no node can die silently. Implementation lives in
/// branch_partition.cpp alongside the shared evaluate_split certificate.
[[nodiscard]] ChildPushStatus
push_branch_children(ThreadSafeNodeQueue& queue, const BranchNode& parent,
                     std::size_t branch_var, double branch_val, double lower_bound,
                     const std::vector<model::Bound>& parent_lower,
                     const std::vector<model::Bound>& parent_upper,
                     const std::optional<lp::dual::BasisState>& warm_basis,
                     std::atomic<std::size_t>& next_node_id);

} // namespace markov_cero::milp
