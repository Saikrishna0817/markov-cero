// MIP-01 contract §4: the production branch-partition construction shared by
// the serial search and the parallel workers. Both engines must produce the
// children the independent replay re-derives (mip_proof_internal.hpp
// children()): down tightens to floor(v), up to ceil(v), each child only when
// its tightened bound still overlaps the parent domain. Degenerate splits and
// empty integer domains return statuses so no node dies without a record.
#include "markov_cero/milp/branch_selector.hpp"
#include "markov_cero/milp/work_queue.hpp"

#include <cmath>
#include <stdexcept>

namespace markov_cero::milp {

SplitPartition evaluate_split(double branch_val, const model::Bound& parent_lower,
                              const model::Bound& parent_upper) {
    SplitPartition split;
    split.floor_value = std::floor(branch_val);
    split.ceil_value = std::ceil(branch_val);
    split.down_valid = !parent_lower.is_finite() ||
                       split.floor_value >= parent_lower.value - 1e-9;
    split.up_valid =
        !parent_upper.is_finite() || split.ceil_value <= parent_upper.value + 1e-9;
    return split;
}

ChildPushStatus push_branch_children(ThreadSafeNodeQueue& queue, const BranchNode& parent,
                                     std::size_t branch_var, double branch_val,
                                     double lower_bound,
                                     const std::vector<model::Bound>& parent_lower,
                                     const std::vector<model::Bound>& parent_upper,
                                     const std::optional<lp::dual::BasisState>& warm_basis,
                                     std::atomic<std::size_t>& next_node_id) {
    if (branch_var >= parent_lower.size() || branch_var >= parent_upper.size()) {
        throw std::invalid_argument("branch variable index is outside node bounds");
    }
    const SplitPartition split =
        evaluate_split(branch_val, parent_lower[branch_var], parent_upper[branch_var]);
    if (!(split.floor_value < split.ceil_value)) return ChildPushStatus::split_rejected;
    if (!split.down_valid && !split.up_valid) {
        queue.note_empty_domain();  // §4.2: record before reporting
        return ChildPushStatus::empty_integer_domain;
    }

    std::shared_ptr<BranchNode> down_child;
    std::shared_ptr<BranchNode> up_child;
    const auto shared_warm_basis = warm_basis
        ? std::make_shared<const lp::dual::BasisState>(*warm_basis)
        : std::shared_ptr<const lp::dual::BasisState>{};

    if (split.down_valid) {
        down_child = std::make_shared<BranchNode>();
        down_child->id = next_node_id.fetch_add(1, std::memory_order_relaxed);
        down_child->parent_id = parent.id;
        down_child->depth = parent.depth + 1;
        down_child->lower_bound = lower_bound;
        down_child->branch_variable = branch_var;
        down_child->branch_value = branch_val;
        down_child->is_down_branch = true;
        down_child->bounds =
            parent.bounds.with_upper(branch_var, model::Bound::finite(split.floor_value));
        down_child->warm_basis = shared_warm_basis;
        down_child->local_cuts = parent.local_cuts;
    }

    if (split.up_valid) {
        up_child = std::make_shared<BranchNode>();
        up_child->id = next_node_id.fetch_add(1, std::memory_order_relaxed);
        up_child->parent_id = parent.id;
        up_child->depth = parent.depth + 1;
        up_child->lower_bound = lower_bound;
        up_child->branch_variable = branch_var;
        up_child->branch_value = branch_val;
        up_child->is_down_branch = false;
        up_child->bounds =
            parent.bounds.with_lower(branch_var, model::Bound::finite(split.ceil_value));
        up_child->warm_basis = shared_warm_basis;
        up_child->local_cuts = parent.local_cuts;
    }

    (void)queue.push_children(std::move(down_child), std::move(up_child));
    return ChildPushStatus::accepted;
}

} // namespace markov_cero::milp
