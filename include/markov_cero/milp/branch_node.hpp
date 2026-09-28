#pragma once

#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/milp/cut_pool.hpp"
#include "markov_cero/milp/node_cuts.hpp"
#include "markov_cero/milp/node_bounds.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace markov_cero::milp {

struct BranchNode {
    std::size_t id{0};
    std::size_t parent_id{0};
    std::size_t depth{0};
    double lower_bound{0.0};
    /// R13/R17: how many times this node's relaxation failed to certify. Used
    /// to retry once cold and to keep an unsolved node from being pruned.
    std::size_t lp_failures{0};
    std::size_t branch_variable{0};
    double branch_value{0.0};
    bool is_down_branch{true};
    NodeBounds bounds;
    std::shared_ptr<const lp::dual::BasisState> warm_basis;
    NodeCuts local_cuts;
};

/// Node-selection policy for the search queue (PS R5: "node selection").
///
/// - `best_bound`: classic best-first — pop the smallest lower bound (min-heap).
///   Minimizes node count but grows the frontier in memory.
/// - `depth_first`: pop the deepest node — pure DFS/diving, depth-bounded memory,
///   finds incumbents early (good for feasibility-first instances).
/// - `best_bound_dive`: best-bound with a depth bonus ("plunge"), the standard
///   hybrid: dive aggressively while bounds are competitive, otherwise fall back
///   to the global best bound.
///
/// The research baseline (Achterberg 2007, "Constraint Integer Programming", §7)
/// evaluates exactly these families; best-bound is optimal for node count, while
/// DFS/plunge trade a modest node increase for memory and early incumbents.
enum class NodeSelection { best_bound, depth_first, best_bound_dive };

/// Relative depth bonus per level used by `best_bound_dive`. The score is
/// `lower_bound - kDiveDepthWeight * max(1, |lower_bound|) * depth`, i.e. the
/// dive bonus scales with the node's own bound magnitude so the policy behaves
/// the same on small-objective and large-objective instances (a fixed absolute
/// bonus would vanish on MIPLIB-scale objectives). 0 makes it exact best-bound.
inline constexpr double kDiveDepthWeight = 3e-2;

/// Policy-aware node comparator. Min-heap semantics: `operator()(a, b) == true`
/// means `a` has *lower* priority than `b` (so `b` is popped first).
struct NodeComparator {
    NodeSelection policy{NodeSelection::best_bound};

    bool operator()(const std::shared_ptr<BranchNode>& a,
                    const std::shared_ptr<BranchNode>& b) const {
        if (!a || !b) {
            return a != nullptr;
        }
        switch (policy) {
            case NodeSelection::depth_first:
                if (a->depth != b->depth) {
                    return a->depth < b->depth;  // deeper node wins
                }
                if (a->lower_bound != b->lower_bound) {
                    return a->lower_bound > b->lower_bound;
                }
                return a->id > b->id;

            case NodeSelection::best_bound_dive: {
                const double score_a =
                    a->lower_bound - kDiveDepthWeight * std::max(1.0, std::abs(a->lower_bound)) *
                                          static_cast<double>(a->depth);
                const double score_b =
                    b->lower_bound - kDiveDepthWeight * std::max(1.0, std::abs(b->lower_bound)) *
                                          static_cast<double>(b->depth);
                if (score_a != score_b) {
                    return score_a > score_b;  // smaller score (deeper/cheaper) wins
                }
                return a->id > b->id;
            }

            case NodeSelection::best_bound:
            default:
                if (a->lower_bound != b->lower_bound) {
                    return a->lower_bound > b->lower_bound;
                }
                return a->depth < b->depth;  // tie-breaker: deeper node first
        }
    }
};

/// Backwards-compatible alias for the historical best-bound-only comparator.
using NodeCompareBestBound = NodeComparator;

} // namespace markov_cero::milp
