#include "markov_cero/milp/work_queue.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <limits>

namespace markov_cero::milp {
namespace {

// Maximum nodes handed out by one pop_batch call. Amortizes queue-lock
// contention over a subtree slice instead of one mutex round-trip per node,
// and removes the per-pop O(n) heap prune that serialized all workers (RW-2).
constexpr std::size_t kDefaultBatchSize = 16;

// Discarded (stale) pops tolerated before the heap is compacted. Keeps the
// heap from retaining null/stale entries without paying a full make_heap on
// every prune request.
constexpr std::size_t kPruneCompactionThreshold = 64;

// Bounded wait so a worker blocked on an empty (but non-quiescent) queue
// re-checks external stop conditions (time/node limits) periodically.
constexpr auto kWaitSlice = std::chrono::milliseconds(2);

} // namespace

void ThreadSafeNodeQueue::set_maximum_size(std::size_t maximum_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!heap_.empty() || active_workers_ != 0)
        throw std::logic_error("queue capacity must be set before search starts");
    maximum_size_ = maximum_size;
}

bool ThreadSafeNodeQueue::push(std::shared_ptr<BranchNode> node) {
    if (!node) {
        return true;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            return false;
        }
        if (heap_.size() >= maximum_size_) {
            capacity_exhausted_ = true;
            minimum_dropped_bound_ = std::min(minimum_dropped_bound_, node->lower_bound);
            return false;
        }
        heap_.push_back(std::move(node));
        std::push_heap(heap_.begin(), heap_.end(), comparator_);
    }
    cv_.notify_one();
    return true;
}

bool ThreadSafeNodeQueue::push_children(std::shared_ptr<BranchNode> left,
                                        std::shared_ptr<BranchNode> right) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            return false;
        }
        const std::size_t required = static_cast<std::size_t>(left != nullptr) +
                                     static_cast<std::size_t>(right != nullptr);
        if (required > maximum_size_ - std::min(maximum_size_, heap_.size())) {
            capacity_exhausted_ = true;
            if (left) minimum_dropped_bound_ = std::min(minimum_dropped_bound_, left->lower_bound);
            if (right) minimum_dropped_bound_ = std::min(minimum_dropped_bound_, right->lower_bound);
            return false;
        }
        bool pushed = false;
        if (left) {
            heap_.push_back(std::move(left));
            std::push_heap(heap_.begin(), heap_.end(), comparator_);
            pushed = true;
        }
        if (right) {
            heap_.push_back(std::move(right));
            std::push_heap(heap_.begin(), heap_.end(), comparator_);
            pushed = true;
        }
        if (!pushed) {
            return true;
        }
    }
    cv_.notify_all();
    return true;
}

bool ThreadSafeNodeQueue::push_branch_children(
    const BranchNode& parent, std::size_t branch_var, double branch_val, double lower_bound,
    const std::vector<model::Bound>& parent_lower,
    const std::vector<model::Bound>& parent_upper,
    const std::optional<lp::dual::BasisState>& warm_basis, std::atomic<std::size_t>& next_node_id) {

    const double floor_val = std::floor(branch_val);
    const double ceil_val = std::ceil(branch_val);

    std::shared_ptr<BranchNode> down_child;
    std::shared_ptr<BranchNode> up_child;
    const auto shared_warm_basis = warm_basis
        ? std::make_shared<const lp::dual::BasisState>(*warm_basis)
        : std::shared_ptr<const lp::dual::BasisState>{};

    if (branch_var >= parent_lower.size() || branch_var >= parent_upper.size()) {
        throw std::invalid_argument("branch variable index is outside node bounds");
    }
    if (!parent_lower[branch_var].is_finite() ||
        floor_val >= parent_lower[branch_var].value - 1e-9) {
        down_child = std::make_shared<BranchNode>();
        down_child->id = next_node_id.fetch_add(1, std::memory_order_relaxed);
        down_child->parent_id = parent.id;
        down_child->depth = parent.depth + 1;
        down_child->lower_bound = lower_bound;
        down_child->branch_variable = branch_var;
        down_child->branch_value = branch_val;
        down_child->is_down_branch = true;
        down_child->bounds = parent.bounds.with_upper(branch_var, model::Bound::finite(floor_val));
        down_child->warm_basis = shared_warm_basis;
    }

    if (!parent_upper[branch_var].is_finite() ||
        ceil_val <= parent_upper[branch_var].value + 1e-9) {
        up_child = std::make_shared<BranchNode>();
        up_child->id = next_node_id.fetch_add(1, std::memory_order_relaxed);
        up_child->parent_id = parent.id;
        up_child->depth = parent.depth + 1;
        up_child->lower_bound = lower_bound;
        up_child->branch_variable = branch_var;
        up_child->branch_value = branch_val;
        up_child->is_down_branch = false;
        up_child->bounds = parent.bounds.with_lower(branch_var, model::Bound::finite(ceil_val));
        up_child->warm_basis = shared_warm_basis;
    }

    return push_children(std::move(down_child), std::move(up_child));
}

std::vector<std::shared_ptr<BranchNode>>
ThreadSafeNodeQueue::pop_batch(bool was_active, double prune_cutoff, bool& became_active,
                               std::size_t max_batch) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (was_active) {
        if (active_workers_ > 0) {
            --active_workers_;
        }
    }
    became_active = false;

    while (!stopped_) {
        // Extract up to max_batch best-bounded live nodes in one critical
        // section. Stale nodes (below the cutoff) are skipped lazily; the
        // heap is compacted only after a discard threshold accumulates.
        std::vector<std::shared_ptr<BranchNode>> batch;
        batch.reserve(max_batch);
        while (!heap_.empty() && batch.size() < max_batch) {
            std::pop_heap(heap_.begin(), heap_.end(), comparator_);
            auto node = std::move(heap_.back());
            heap_.pop_back();
            if (!node || node->lower_bound >= prune_cutoff) {
                ++prune_lazily_discarded_;
                continue;
            }
            batch.push_back(std::move(node));
        }

        if (prune_lazily_discarded_ >= kPruneCompactionThreshold && heap_.size() > 16) {
            heap_.erase(std::remove_if(heap_.begin(), heap_.end(),
                                       [](const std::shared_ptr<BranchNode>& n) { return !n; }),
                        heap_.end());
            std::make_heap(heap_.begin(), heap_.end(), comparator_);
            prune_lazily_discarded_ = 0;
        }

        if (!batch.empty()) {
            active_workers_ += 1; // one activity claim per batch (per worker)
            became_active = true;
            return batch;
        }

        // Heap drained. Terminate only when no worker holds unprocessed work.
        if (active_workers_ == 0) {
            stopped_ = true;
            cv_.notify_all();
            return batch; // empty
        }

        // Work may still be in flight elsewhere: wait briefly and re-check.
        // prune_cutoff staleness across waits is benign: the incumbent only
        // improves, so a looser cutoff can at worst surface an already-pruned
        // node, which the caller re-checks against the live incumbent anyway.
        cv_.wait_for(lock, kWaitSlice);
    }

    return {};
}

void ThreadSafeNodeQueue::prune_locked(double cutoff) {
    if (heap_.empty()) {
        return;
    }
    auto it = std::remove_if(heap_.begin(), heap_.end(),
                             [cutoff](const std::shared_ptr<BranchNode>& node) {
                                 return !node || node->lower_bound >= cutoff;
                             });
    if (it != heap_.end()) {
        heap_.erase(it, heap_.end());
        std::make_heap(heap_.begin(), heap_.end(), comparator_);
    }
}

void ThreadSafeNodeQueue::prune(double cutoff) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        prune_locked(cutoff);
    }
    cv_.notify_all();
}

void ThreadSafeNodeQueue::deactivate_worker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_workers_ > 0) {
            --active_workers_;
        }
        if (active_workers_ == 0 && heap_.empty()) {
            stopped_ = true;
        }
    }
    cv_.notify_all();
}

void ThreadSafeNodeQueue::request_stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
}

bool ThreadSafeNodeQueue::is_stopped() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stopped_;
}

bool ThreadSafeNodeQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heap_.empty();
}

std::size_t ThreadSafeNodeQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heap_.size();
}

bool ThreadSafeNodeQueue::capacity_exhausted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capacity_exhausted_;
}

std::size_t ThreadSafeNodeQueue::active_workers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_workers_;
}

double ThreadSafeNodeQueue::min_lower_bound() const {
    std::lock_guard<std::mutex> lock(mutex_);
    double bound = minimum_dropped_bound_;
    if (heap_.empty()) return bound;
    // Under best_bound ordering the heap front is the minimum-bound node;
    // under depth_first / best_bound_dive it is the deepest / dive-scored
    // node, and using it as the frontier bound would close the optimality
    // gap spuriously (false "Optimal" — same R13/R17 hazard as the
    // sequential NodeFrontier). Other policies scan the heap.
    if (comparator_.policy == NodeSelection::best_bound) {
        return std::min(bound, heap_.front()->lower_bound);
    }
    for (const auto& node : heap_) {
        if (node && node->lower_bound < bound) {
            bound = node->lower_bound;
        }
    }
    return bound;
}

void ThreadSafeNodeQueue::notify_all() { cv_.notify_all(); }

} // namespace markov_cero::milp
