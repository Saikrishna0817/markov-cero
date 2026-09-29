#include "markov_cero/milp/reference_materialisation.hpp"

#include <algorithm>
#include <cmath>
#include <new>
#include <stdexcept>

namespace markov_cero::milp {
namespace {

constexpr std::size_t kNoIndex = static_cast<std::size_t>(-1);
constexpr std::size_t kBoundPairBytes = 2U * sizeof(model::Bound);

ReferenceComparison result_of(ReferenceStatus status, ReferenceFacet facet = ReferenceFacet::none,
                              std::size_t index = kNoIndex, std::size_t bytes = 0) {
    ReferenceComparison out;
    out.status = status;
    out.facet = facet;
    out.first_mismatch_index = index;
    out.reference_bytes = bytes;
    return out;
}

bool bounds_equal(const model::Bound& a, const model::Bound& b) noexcept {
    if (a.kind != b.kind) return false;
    return a.value == b.value || (std::isnan(a.value) && std::isnan(b.value));
}

/// Caps and input validation shared by both entry points. Refusal and
/// invalidity are decided before any allocation, so `reference_bytes` stays
/// zero exactly when nothing was allocated.
ReferenceComparison preflight(const std::vector<model::Bound>& root_lower,
                              const std::vector<model::Bound>& root_upper,
                              const std::vector<ReferenceStep>& steps,
                              const ReferenceCaps& caps) {
    if (root_lower.size() != root_upper.size()) return result_of(ReferenceStatus::invalid);
    if (steps.size() > caps.max_steps) return result_of(ReferenceStatus::refused);
    if (root_lower.size() > caps.max_variables) return result_of(ReferenceStatus::refused);
    if (root_lower.size() > caps.max_bytes / kBoundPairBytes)
        return result_of(ReferenceStatus::refused);
    std::size_t total_cuts = 0;
    for (const auto& step : steps) {
        if (step.variable >= root_lower.size()) return result_of(ReferenceStatus::invalid);
        if (step.local_cuts.size() > caps.max_steps - total_cuts)
            return result_of(ReferenceStatus::refused);
        total_cuts += step.local_cuts.size();
    }
    return result_of(ReferenceStatus::ok, ReferenceFacet::none, kNoIndex,
                     root_lower.size() * kBoundPairBytes);
}

/// The baseline reference algorithm: copy the root bounds, then replay every
/// recorded step chronologically. Production instead walks the delta chain
/// newest to oldest, so agreement is evidence about the structure, not about
/// a shared code path.
ReferenceComparison materialise_reference(const std::vector<model::Bound>& root_lower,
                                          const std::vector<model::Bound>& root_upper,
                                          const std::vector<ReferenceStep>& steps,
                                          std::vector<model::Bound>& lower_out,
                                          std::vector<model::Bound>& upper_out,
                                          std::size_t bytes) {
    try {
        lower_out = root_lower;
        upper_out = root_upper;
    } catch (const std::exception&) {
        return result_of(ReferenceStatus::allocation_failed);
    }
    for (const auto& step : steps) {
        if (step.lower) lower_out[step.variable] = *step.lower;
        if (step.upper) upper_out[step.variable] = *step.upper;
    }
    return result_of(ReferenceStatus::ok, ReferenceFacet::none, kNoIndex, bytes);
}

/// Element-wise comparison of the full bound vectors, lower before upper per
/// index, first divergence reported with its facet and index.
ReferenceComparison compare_bounds(const std::vector<model::Bound>& reference_lower,
                                   const std::vector<model::Bound>& reference_upper,
                                   const std::vector<model::Bound>& production_lower,
                                   const std::vector<model::Bound>& production_upper,
                                   std::size_t reference_bytes) {
    if (production_lower.size() != reference_lower.size() ||
        production_upper.size() != reference_upper.size()) {
        return result_of(ReferenceStatus::mismatch, ReferenceFacet::production, kNoIndex,
                         reference_bytes);
    }
    for (std::size_t i = 0; i < reference_lower.size(); ++i) {
        if (!bounds_equal(reference_lower[i], production_lower[i]))
            return result_of(ReferenceStatus::mismatch, ReferenceFacet::lower_bounds, i,
                             reference_bytes);
        if (!bounds_equal(reference_upper[i], production_upper[i]))
            return result_of(ReferenceStatus::mismatch, ReferenceFacet::upper_bounds, i,
                             reference_bytes);
    }
    return result_of(ReferenceStatus::ok, ReferenceFacet::none, kNoIndex, reference_bytes);
}

ReferenceComparison compare_cuts(const std::vector<NodeView::CutId>& reference,
                                 const std::vector<NodeView::CutId>& production,
                                 std::size_t reference_bytes) {
    const std::size_t common = std::min(reference.size(), production.size());
    for (std::size_t i = 0; i < common; ++i) {
        if (reference[i] != production[i])
            return result_of(ReferenceStatus::mismatch, ReferenceFacet::cut_scope, i,
                             reference_bytes);
    }
    if (reference.size() != production.size())
        return result_of(ReferenceStatus::mismatch, ReferenceFacet::cut_scope, common,
                         reference_bytes);
    return result_of(ReferenceStatus::ok, ReferenceFacet::none, kNoIndex, reference_bytes);
}

} // namespace

ReferenceComparison compare_bounds_to_reference(const NodeBounds& bounds,
                                                 const std::vector<model::Bound>& root_lower,
                                                 const std::vector<model::Bound>& root_upper,
                                                 const std::vector<ReferenceStep>& steps,
                                                 const ReferenceCaps& caps) {
    ReferenceComparison check = preflight(root_lower, root_upper, steps, caps);
    if (!check.ok()) return check;
    const std::size_t bytes = check.reference_bytes;
    if (!bounds.indices_within(root_lower.size()))
        return result_of(ReferenceStatus::mismatch, ReferenceFacet::production);

    std::vector<model::Bound> reference_lower;
    std::vector<model::Bound> reference_upper;
    ReferenceComparison reference =
        materialise_reference(root_lower, root_upper, steps, reference_lower, reference_upper,
                              bytes);
    if (!reference.ok()) return reference;

    std::vector<model::Bound> production_lower;
    std::vector<model::Bound> production_upper;
    try {
        bounds.materialize(root_lower, root_upper, production_lower, production_upper);
    } catch (const std::invalid_argument&) {
        return result_of(ReferenceStatus::mismatch, ReferenceFacet::production);
    } catch (const std::exception&) {
        return result_of(ReferenceStatus::allocation_failed);
    }
    return compare_bounds(reference_lower, reference_upper, production_lower, production_upper,
                          bytes);
}

ReferenceComparison compare_view_to_reference(const NodeView& view,
                                               const std::vector<model::Bound>& root_lower,
                                               const std::vector<model::Bound>& root_upper,
                                               const std::vector<ReferenceStep>& steps,
                                               const ReferenceCaps& caps) {
    ReferenceComparison check = preflight(root_lower, root_upper, steps, caps);
    if (!check.ok()) return check;
    const std::size_t bytes = check.reference_bytes;
    if (!view.bounds().indices_within(root_lower.size()))
        return result_of(ReferenceStatus::mismatch, ReferenceFacet::production);

    std::vector<model::Bound> reference_lower;
    std::vector<model::Bound> reference_upper;
    ReferenceComparison reference =
        materialise_reference(root_lower, root_upper, steps, reference_lower, reference_upper,
                              bytes);
    if (!reference.ok()) return reference;

    std::vector<model::Bound> production_lower;
    std::vector<model::Bound> production_upper;
    core::MemoryBudget budget(bytes);
    NodeBounds::MaterializationScratch scratch;
    const MaterializationResult produced = view.materialize(
        root_lower, root_upper, production_lower, production_upper, scratch, budget);
    NodeView::release(produced, budget);
    if (produced.status == MaterializationStatus::invalid_dimensions)
        return result_of(ReferenceStatus::mismatch, ReferenceFacet::production);
    if (!produced.ok()) return result_of(ReferenceStatus::allocation_failed);

    ReferenceComparison bounds =
        compare_bounds(reference_lower, reference_upper, production_lower, production_upper,
                       bytes);
    if (!bounds.ok()) return bounds;

    std::vector<NodeView::CutId> reference_cuts;
    try {
        std::size_t total = 0;
        for (const auto& step : steps) total += step.local_cuts.size();
        reference_cuts.reserve(total);
        for (const auto& step : steps)
            reference_cuts.insert(reference_cuts.end(), step.local_cuts.begin(),
                                  step.local_cuts.end());
    } catch (const std::exception&) {
        return result_of(ReferenceStatus::allocation_failed);
    }
    std::vector<NodeView::CutId> production_cuts;
    try {
        view.append_scoped_cut_ids(production_cuts);
    } catch (const std::exception&) {
        return result_of(ReferenceStatus::allocation_failed);
    }
    return compare_cuts(reference_cuts, production_cuts, bytes);
}

} // namespace markov_cero::milp
