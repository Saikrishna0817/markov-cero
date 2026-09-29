#include "markov_cero/milp/node_view.hpp"

#include <stdexcept>

namespace markov_cero::milp {
namespace {

const std::vector<NodeView::CutId>& empty_cuts() noexcept {
    static const std::vector<NodeView::CutId> none;
    return none;
}

const std::shared_ptr<const lp::dual::BasisState>& empty_basis() noexcept {
    static const std::shared_ptr<const lp::dual::BasisState> none;
    return none;
}

} // namespace

std::shared_ptr<const NodeView> NodeView::root() {
    return std::shared_ptr<const NodeView>(
        new NodeView(nullptr, 0, NodeBounds{}, nullptr, nullptr, LowerBoundEvidence::none()));
}

std::shared_ptr<const NodeView> NodeView::child(const Contribution& contribution) const {
    NodeBounds bounds = bounds_;
    if (contribution.tightened_lower)
        bounds = bounds.with_lower(contribution.tightened_lower->first,
                                   contribution.tightened_lower->second);
    if (contribution.tightened_upper)
        bounds = bounds.with_upper(contribution.tightened_upper->first,
                                   contribution.tightened_upper->second);

    std::shared_ptr<const std::vector<CutId>> local_cuts;
    if (!contribution.local_cuts.empty())
        local_cuts = std::make_shared<const std::vector<CutId>>(contribution.local_cuts);

    // Basis metadata: an explicit child basis wins; otherwise the child keeps
    // sharing the parent's handle rather than copying basis vectors.
    std::shared_ptr<const lp::dual::BasisState> basis =
        contribution.basis ? contribution.basis : basis_;

    LowerBoundEvidence evidence;
    if (contribution.lower_bound) {
        evidence = *contribution.lower_bound;
    } else if (evidence_.usable()) {
        // An ancestor's certified bound remains valid for this child, but the
        // provenance is restated as inherited rather than as this node's own
        // relaxation result.
        evidence = LowerBoundEvidence::inherited(evidence_.value);
    } else {
        evidence = LowerBoundEvidence::none();
    }

    return std::shared_ptr<const NodeView>(
        new NodeView(shared_from_this(), depth_ + 1, std::move(bounds), std::move(local_cuts),
                     std::move(basis), evidence));
}

const std::vector<NodeView::CutId>& NodeView::local_cut_ids() const noexcept {
    return local_cuts_ ? *local_cuts_ : empty_cuts();
}

void NodeView::append_scoped_cut_ids(std::vector<CutId>& out) const {
    if (parent_) parent_->append_scoped_cut_ids(out);
    if (local_cuts_) out.insert(out.end(), local_cuts_->begin(), local_cuts_->end());
}

bool NodeView::cut_in_scope(CutId id) const noexcept {
    for (const NodeView* node = this; node; node = node->parent_.get()) {
        if (!node->local_cuts_) continue;
        for (const CutId candidate : *node->local_cuts_) {
            if (candidate == id) return true;
        }
    }
    return false;
}

const std::shared_ptr<const lp::dual::BasisState>& NodeView::effective_basis() const noexcept {
    for (const NodeView* node = this; node; node = node->parent_.get()) {
        if (node->basis_) return node->basis_;
    }
    return empty_basis();
}

MaterializationResult NodeView::materialize(const std::vector<model::Bound>& root_lower,
                                            const std::vector<model::Bound>& root_upper,
                                            std::vector<model::Bound>& lower_out,
                                            std::vector<model::Bound>& upper_out,
                                            NodeBounds::MaterializationScratch& scratch,
                                            core::MemoryBudget& budget) const {
    if (root_lower.size() != root_upper.size())
        return {MaterializationStatus::invalid_dimensions, 0};
    if (&lower_out == &upper_out || &root_lower == &lower_out || &root_lower == &upper_out ||
        &root_upper == &lower_out || &root_upper == &upper_out)
        return {MaterializationStatus::invalid_dimensions, 0};
    if (!bounds_.indices_within(root_lower.size()))
        return {MaterializationStatus::invalid_dimensions, 0};

    const std::size_t bytes = materialization_bytes(root_lower.size());
    if (!budget.try_charge(bytes)) return {MaterializationStatus::budget_exhausted, 0};

    // Reserve first so a host allocation failure surfaces before any output is
    // rewritten; NodeBounds then builds its delta path (its last allocation)
    // and finally writes both vectors exactly once.
    try {
        lower_out.reserve(root_lower.size());
        upper_out.reserve(root_upper.size());
        bounds_.materialize(root_lower, root_upper, lower_out, upper_out, scratch);
    } catch (const std::invalid_argument&) {
        budget.release(bytes);
        return {MaterializationStatus::invalid_dimensions, 0};
    } catch (const std::exception&) {
        // Host allocation failure (std::bad_alloc) or a capacity refusal
        // (std::length_error): fail closed with the charge released and the
        // caller's output contents untouched. Reported separately from budget
        // exhaustion so the caller can tell "solve budget" from "machine".
        budget.release(bytes);
        return {MaterializationStatus::allocation_failed, 0};
    }
    return {MaterializationStatus::ok, bytes};
}

void NodeView::release(MaterializationResult result, core::MemoryBudget& budget) noexcept {
    if (result.ok() && result.charged_bytes != 0) budget.release(result.charged_bytes);
}

} // namespace markov_cero::milp
