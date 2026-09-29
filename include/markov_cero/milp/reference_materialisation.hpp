#pragma once

#include "markov_cero/core/memory_budget.hpp"
#include "markov_cero/milp/node_bounds.hpp"
#include "markov_cero/milp/node_view.hpp"
#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace markov_cero::milp {

/// Caps for the bounded reference-materialisation oracle (W01/IR-19).
///
/// The oracle is a regression tool, never a solver path: it refuses before
/// allocating when a request exceeds any cap, so a corrupt chain can never
/// turn the check itself into an unbounded allocation.
struct ReferenceCaps final {
    std::size_t max_variables{1U << 20};
    std::size_t max_steps{1U << 16};
    std::size_t max_bytes{64U << 20};
};

enum class ReferenceStatus : std::uint8_t {
    ok = 0,
    /// Production and reference disagree; `facet`/`first_mismatch_index` say where.
    mismatch,
    /// Request exceeded `ReferenceCaps`; nothing was allocated.
    refused,
    /// Caller inputs malformed (root dimensions differ, step index out of range).
    invalid,
    /// The bounded reference allocation itself failed; fail closed.
    allocation_failed,
};

/// Which materialised surface diverged first.
enum class ReferenceFacet : std::uint8_t {
    none = 0,
    lower_bounds,
    upper_bounds,
    cut_scope,
    /// The production structure itself was malformed (delta index out of
    /// range); `first_mismatch_index` is `npos` because no variable can be named.
    production,
};

struct ReferenceComparison final {
    ReferenceStatus status{ReferenceStatus::invalid};
    ReferenceFacet facet{ReferenceFacet::none};
    std::size_t first_mismatch_index{static_cast<std::size_t>(-1)};
    /// Bytes the reference path allocated (0 whenever it refused or was invalid).
    std::size_t reference_bytes{0};

    [[nodiscard]] bool ok() const noexcept { return status == ReferenceStatus::ok; }
};

/// One recorded branching step, replayed by the oracle against the root
/// bounds. The caller owns the script; the production structures own only
/// their deltas, which is exactly what the comparison checks.
struct ReferenceStep final {
    std::size_t variable{};
    std::optional<model::Bound> lower;
    std::optional<model::Bound> upper;
    std::vector<NodeView::CutId> local_cuts;
};

/// Bounded reference materialisation of a persistent `NodeBounds` chain.
///
/// The reference algorithm is deliberately different from production: it
/// copies the root vectors and replays every recorded step in chronological
/// order, while `NodeBounds::materialize` walks the delta chain newest to
/// oldest. Element-wise equality of the two full vectors is then evidence
/// that the shared structure, not a shared algorithm, produced them.
[[nodiscard]] ReferenceComparison compare_bounds_to_reference(
    const NodeBounds& bounds,
    const std::vector<model::Bound>& root_lower,
    const std::vector<model::Bound>& root_upper,
    const std::vector<ReferenceStep>& steps,
    const ReferenceCaps& caps = {});

/// Same oracle for the immutable view API: bounds as above, plus the scoped
/// cut list against the concatenation of the script's local cuts. Production
/// materialisation runs through the budgeted `NodeView::materialize` on an
/// internal budget sized exactly for this comparison.
[[nodiscard]] ReferenceComparison compare_view_to_reference(
    const NodeView& view,
    const std::vector<model::Bound>& root_lower,
    const std::vector<model::Bound>& root_upper,
    const std::vector<ReferenceStep>& steps,
    const ReferenceCaps& caps = {});

} // namespace markov_cero::milp
