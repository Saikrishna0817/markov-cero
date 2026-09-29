#pragma once

#include "markov_cero/core/memory_budget.hpp"
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/milp/node_bounds.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace markov_cero::milp {

/// Provenance of a node's lower-bound evidence (M4/W01 bound audit).
///
/// Pruning requires a certified value: `unknown` evidence carries NaN and can
/// never justify closing a region, so a failed or unsolved relaxation stays
/// "unknown" instead of borrowing a primal objective as a bound.
enum class BoundEvidenceSource : std::uint8_t {
    unknown = 0,
    certified_relaxation, // solved relaxation lower bound of this node
    inherited,            // an ancestor's certified bound, valid for this child
    propagated,           // bound propagation / implied bound with a derivation
};

struct LowerBoundEvidence final {
    BoundEvidenceSource source{BoundEvidenceSource::unknown};
    double value{std::numeric_limits<double>::quiet_NaN()};

    [[nodiscard]] bool usable() const noexcept {
        return source != BoundEvidenceSource::unknown && std::isfinite(value);
    }

    [[nodiscard]] static LowerBoundEvidence none() noexcept { return {}; }

    [[nodiscard]] static LowerBoundEvidence certified(double bound) noexcept {
        return {BoundEvidenceSource::certified_relaxation, bound};
    }

    [[nodiscard]] static LowerBoundEvidence inherited(double bound) noexcept {
        return {BoundEvidenceSource::inherited, bound};
    }

    [[nodiscard]] static LowerBoundEvidence derived(double bound) noexcept {
        return {BoundEvidenceSource::propagated, bound};
    }
};

/// Outcome of an explicit, budgeted materialization (D05/W01).
///
/// Preflight failures (`budget_exhausted` from the charge, or
/// `invalid_dimensions` from the shape/index checks) leave the output vectors
/// untouched, so a rejected materialization can never hand a stale or
/// half-updated model to an engine. A host allocation failure during the write
/// is reported as `budget_exhausted` with the charge returned; outputs are
/// then unspecified and the caller must not read them.
enum class MaterializationStatus : std::uint8_t {
    ok = 0,
    budget_exhausted,   // charge refused; nothing was written
    invalid_dimensions, // root vectors disagree or a delta index is out of range
    // The host allocator refused (or threw) while materializing. Distinct
    // from `budget_exhausted`: the solve budget still had room, the machine
    // did not. Nothing was written and the charge was released.
    allocation_failed,
};

struct MaterializationResult final {
    MaterializationStatus status{MaterializationStatus::invalid_dimensions};
    std::size_t charged_bytes{0};

    [[nodiscard]] bool ok() const noexcept { return status == MaterializationStatus::ok; }
};

/// Immutable view of one branch-and-bound node (W01 contract).
///
/// A view stores only what *this* node added: its bound delta, its locally
/// separated cuts, optional evidence and optional basis metadata. Everything
/// else is shared with the parent, so queued children never own a copy of the
/// model or of ancestor state (D05/M5: frontier growth must not multiply
/// matrix size).
///
/// Ownership: views are always owned by `shared_ptr` and a child keeps its
/// parent alive, so parent links stay valid across the whole tree lifetime,
/// including across worker threads. Views are immutable after construction;
/// a "change" creates a child. A view copied out of its `shared_ptr` (value
/// copy) cannot create children and throws `std::bad_weak_ptr`.
///
/// Materialization into dense bound vectors is explicit and charged to a
/// memory budget — the only way to turn a view into per-node arrays.
class NodeView final : public std::enable_shared_from_this<NodeView> {
  public:
    using CutId = std::uint64_t;

    /// What one branching step contributes to its child.
    struct Contribution final {
        std::optional<std::pair<std::size_t, model::Bound>> tightened_lower;
        std::optional<std::pair<std::size_t, model::Bound>> tightened_upper;
        std::vector<CutId> local_cuts;
        /// nullptr = the child reuses the parent's basis metadata.
        std::shared_ptr<const lp::dual::BasisState> basis;
        /// nullopt = the child inherits the parent's evidence.
        std::optional<LowerBoundEvidence> lower_bound;
    };

    [[nodiscard]] static std::shared_ptr<const NodeView> root();

    /// Returns a child sharing this view's state. Requires this view to be
    /// owned by a `shared_ptr`, which every view from `root()`/`child()` is.
    [[nodiscard]] std::shared_ptr<const NodeView> child(const Contribution& contribution) const;

    [[nodiscard]] const NodeView* parent() const noexcept { return parent_.get(); }
    [[nodiscard]] std::size_t depth() const noexcept { return depth_; }
    [[nodiscard]] const NodeBounds& bounds() const noexcept { return bounds_; }
    [[nodiscard]] std::size_t bound_delta_count() const noexcept {
        return bounds_.delta_count();
    }

    [[nodiscard]] const std::vector<CutId>& local_cut_ids() const noexcept;
    /// Cuts in scope at this node: ancestors' local cuts first, then its own.
    /// A sibling's cuts never appear, so cut scope is structural rather than
    /// a convention.
    void append_scoped_cut_ids(std::vector<CutId>& out) const;
    [[nodiscard]] bool cut_in_scope(CutId id) const noexcept;

    /// Nearest non-null basis metadata walking up the parent chain.
    [[nodiscard]] const std::shared_ptr<const lp::dual::BasisState>& effective_basis()
        const noexcept;

    [[nodiscard]] const LowerBoundEvidence& lower_bound_evidence() const noexcept {
        return evidence_;
    }

    /// Bytes a full materialization of this view's bound vectors needs
    /// (2 · n · sizeof(Bound)); the amount charged to the budget on success.
    [[nodiscard]] std::size_t materialization_bytes(std::size_t variable_count) const noexcept {
        return 2U * sizeof(model::Bound) * variable_count;
    }

    [[nodiscard]] MaterializationResult materialize(
        const std::vector<model::Bound>& root_lower,
        const std::vector<model::Bound>& root_upper,
        std::vector<model::Bound>& lower_out,
        std::vector<model::Bound>& upper_out,
        NodeBounds::MaterializationScratch& scratch,
        core::MemoryBudget& budget) const;

    /// Releases a successful materialization's charge. Call it when the
    /// output vectors are freed.
    static void release(MaterializationResult result, core::MemoryBudget& budget) noexcept;

  private:
    NodeView(std::shared_ptr<const NodeView> parent, std::size_t depth, NodeBounds bounds,
             std::shared_ptr<const std::vector<CutId>> local_cuts,
             std::shared_ptr<const lp::dual::BasisState> basis, LowerBoundEvidence evidence)
        : parent_(std::move(parent)), depth_(depth), bounds_(std::move(bounds)),
          local_cuts_(std::move(local_cuts)), basis_(std::move(basis)), evidence_(evidence) {}

    std::shared_ptr<const NodeView> parent_;
    std::size_t depth_{0};
    NodeBounds bounds_;
    std::shared_ptr<const std::vector<CutId>> local_cuts_;
    std::shared_ptr<const lp::dual::BasisState> basis_;
    LowerBoundEvidence evidence_{};
};

} // namespace markov_cero::milp
