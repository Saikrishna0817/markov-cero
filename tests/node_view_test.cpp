#include "markov_cero/milp/node_view.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using markov_cero::core::MemoryBudget;
using markov_cero::model::Bound;
using markov_cero::milp::BoundEvidenceSource;
using markov_cero::milp::LowerBoundEvidence;
using markov_cero::milp::MaterializationStatus;
using markov_cero::milp::NodeBounds;
using markov_cero::milp::NodeView;
using Contribution = NodeView::Contribution;
using CutId = NodeView::CutId;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool same_bounds(const std::vector<Bound>& left, const std::vector<Bound>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].kind != right[index].kind) return false;
        if (left[index].kind == markov_cero::model::BoundKind::finite &&
            left[index].value != right[index].value)
            return false;
    }
    return true;
}

std::vector<Bound> constant_bounds(std::size_t count, double value) {
    std::vector<Bound> bounds(count);
    for (Bound& bound : bounds) bound = Bound::finite(value);
    return bounds;
}

std::shared_ptr<const markov_cero::lp::dual::BasisState> make_basis(std::size_t rows) {
    auto state = std::make_shared<markov_cero::lp::dual::BasisState>();
    state->rows = rows;
    state->columns = rows;
    state->model_fingerprint = "basis";
    return state;
}

void test_root_view() {
    const auto root = NodeView::root();
    require(root->depth() == 0, "root depth is zero");
    require(root->parent() == nullptr, "root has no parent");
    require(root->bound_delta_count() == 0, "root records no bound delta");
    require(root->local_cut_ids().empty(), "root starts without local cuts");
    require(!root->lower_bound_evidence().usable(), "root starts without evidence");
    require(std::isnan(root->lower_bound_evidence().value), "unused evidence carries NaN");
    require(!root->effective_basis(), "root carries no basis metadata");
    std::vector<CutId> scoped;
    root->append_scoped_cut_ids(scoped);
    require(scoped.empty(), "root scope is empty");
    require(!root->cut_in_scope(1), "an empty scope contains nothing");
}

void test_child_shares_and_records() {
    const auto root = NodeView::root();
    Contribution contribution;
    contribution.tightened_lower = std::make_pair<std::size_t, Bound>(0, Bound::finite(2.0));
    contribution.tightened_upper = std::make_pair<std::size_t, Bound>(1, Bound::finite(7.0));
    contribution.local_cuts = {11};
    const auto child = root->child(contribution);
    require(child->depth() == 1, "child depth follows the parent");
    require(child->parent() == root.get(), "child keeps the exact parent identity");
    require(child->bound_delta_count() == 2, "both bound changes are recorded once");
    require(child->local_cut_ids().size() == 1 && child->local_cut_ids()[0] == 11,
            "child records its own cut");
    require(root->local_cut_ids().empty(), "the parent stays unchanged");
    require(child->bounds().delta_count() == 2, "child bounds are the persistent overlay");
}

void test_sibling_cut_scope_is_structural() {
    const auto root = NodeView::root();
    Contribution with_parent_cut;
    with_parent_cut.local_cuts = {7};
    const auto parent = root->child(with_parent_cut);

    Contribution left;
    left.local_cuts = {11};
    Contribution right;
    right.local_cuts = {22};
    const auto left_child = parent->child(left);
    const auto right_child = parent->child(right);

    require(left_child->parent() == right_child->parent(),
            "siblings share one parent object");
    std::vector<CutId> left_scope;
    left_child->append_scoped_cut_ids(left_scope);
    require(left_scope.size() == 2 && left_scope[0] == 7 && left_scope[1] == 11,
            "scope runs root-first with the parent cut first");
    std::vector<CutId> right_scope;
    right_child->append_scoped_cut_ids(right_scope);
    require(right_scope.size() == 2 && right_scope[0] == 7 && right_scope[1] == 22,
            "the sibling keeps its own cut list");
    require(!left_child->cut_in_scope(22),
            "a sibling's cut is never in this node's scope");
    require(!right_child->cut_in_scope(11), "scope does not leak sideways");
    require(left_child->cut_in_scope(7) && right_child->cut_in_scope(7),
            "parent cuts are in both scopes");
    require(parent->local_cut_ids().size() == 1, "the parent's list is untouched");
}

void test_basis_and_evidence_inheritance() {
    const auto root = NodeView::root();
    Contribution base;
    base.basis = make_basis(4);
    base.lower_bound = LowerBoundEvidence::certified(-3.5);
    const auto certified = root->child(base);
    require(certified->lower_bound_evidence().source ==
                BoundEvidenceSource::certified_relaxation,
            "an explicit relaxation bound keeps its provenance");
    require(certified->effective_basis() && certified->effective_basis()->rows == 4,
            "the child uses the basis it was given");

    Contribution plain;
    const auto inherited = certified->child(plain);
    require(inherited->lower_bound_evidence().source == BoundEvidenceSource::inherited,
            "a child restates an ancestor bound as inherited");
    require(inherited->lower_bound_evidence().value == -3.5,
            "the inherited value is preserved");
    require(inherited->effective_basis() == certified->effective_basis(),
            "basis metadata is shared up the chain");

    Contribution replacement;
    replacement.basis = make_basis(9);
    replacement.lower_bound = LowerBoundEvidence::none();
    const auto replaced = inherited->child(replacement);
    require(replaced->effective_basis() != inherited->effective_basis(),
            "an explicit basis overrides inheritance");
    require(replaced->effective_basis()->rows == 9, "the overriding basis is the child's own");
    require(!replaced->lower_bound_evidence().usable(),
            "an explicit unknown clears inherited evidence");
    require(std::isnan(replaced->lower_bound_evidence().value),
            "cleared evidence carries NaN, not a stale number");

    Contribution propagated;
    propagated.lower_bound = LowerBoundEvidence::derived(-1.25);
    const auto derived = root->child(propagated);
    require(derived->lower_bound_evidence().source == BoundEvidenceSource::propagated,
            "propagated evidence keeps its provenance");
    require(derived->lower_bound_evidence().usable(), "propagated evidence is usable");
}

void test_materialization_matches_full_reconstruction() {
    std::mt19937_64 random(0x4e4f444556494557ULL);
    const std::size_t variables = 6;
    const std::vector<Bound> root_lower = constant_bounds(variables, 0.0);
    const std::vector<Bound> root_upper = constant_bounds(variables, 10.0);

    for (int trial = 0; trial < 128; ++trial) {
        auto node = NodeView::root();
        std::vector<Bound> expected_lower = root_lower;
        std::vector<Bound> expected_upper = root_upper;
        for (int step = 0; step < 8; ++step) {
            const std::size_t variable = static_cast<std::size_t>(random() % variables);
            Contribution contribution;
            if (random() & 1U) {
                const double value = static_cast<double>(random() % 100U) / 10.0;
                contribution.tightened_lower = std::make_pair(variable, Bound::finite(value));
                expected_lower[variable] = Bound::finite(value);
            } else {
                const double value = static_cast<double>(random() % 100U) / 10.0;
                contribution.tightened_upper = std::make_pair(variable, Bound::finite(value));
                expected_upper[variable] = Bound::finite(value);
            }
            node = node->child(contribution);
        }

        MemoryBudget budget(1U << 20U);
        NodeBounds::MaterializationScratch scratch;
        std::vector<Bound> lower_out;
        std::vector<Bound> upper_out;
        const auto result =
            node->materialize(root_lower, root_upper, lower_out, upper_out, scratch, budget);
        require(result.ok(), "materialization inside a sufficient budget succeeds");
        require(result.charged_bytes == node->materialization_bytes(variables),
                "the charge matches the declared materialization size");
        require(budget.charged() == result.charged_bytes, "the charge is recorded");
        require(same_bounds(lower_out, expected_lower) &&
                same_bounds(upper_out, expected_upper),
                "materialization matches the full reconstruction");
        NodeView::release(result, budget);
        require(budget.charged() == 0, "release returns the budget");
    }
}

void test_materialization_preflight_failures() {
    const std::size_t variables = 4;
    const std::vector<Bound> root_lower = constant_bounds(variables, 0.0);
    const std::vector<Bound> root_upper = constant_bounds(variables, 5.0);
    auto node = NodeView::root();
    Contribution contribution;
    contribution.tightened_lower = std::make_pair<std::size_t, Bound>(1, Bound::finite(1.0));
    node = node->child(contribution);

    NodeBounds::MaterializationScratch scratch;
    std::vector<Bound> lower_out(3, Bound::finite(-1.0));
    std::vector<Bound> upper_out(3, Bound::finite(-1.0));

    MemoryBudget short_budget(node->materialization_bytes(variables) - 1U);
    const auto refused =
        node->materialize(root_lower, root_upper, lower_out, upper_out, scratch, short_budget);
    require(refused.status == MaterializationStatus::budget_exhausted,
            "an insufficient budget is refused");
    require(refused.charged_bytes == 0, "a refused materialization charges nothing");
    require(short_budget.charged() == 0, "nothing stays charged after a refusal");
    require(lower_out[0].value == -1.0 && upper_out[0].value == -1.0,
            "a refused materialization leaves the outputs untouched");

    const std::vector<Bound> short_upper(root_upper.begin(), root_upper.end() - 1);
    MemoryBudget enough(node->materialization_bytes(variables));
    const auto bad_shape = node->materialize(root_lower, short_upper, lower_out, upper_out,
                                             scratch, enough);
    require(bad_shape.status == MaterializationStatus::invalid_dimensions,
            "mismatched root vectors are rejected");
    require(enough.charged() == 0, "a rejected shape charges nothing");

    const auto aliased = node->materialize(root_lower, root_upper, lower_out, lower_out,
                                           scratch, enough);
    require(aliased.status == MaterializationStatus::invalid_dimensions,
            "aliasing output vectors is rejected");

    Contribution out_of_range;
    out_of_range.tightened_lower = std::make_pair<std::size_t, Bound>(9, Bound::finite(1.0));
    const auto corrupt = node->child(out_of_range);
    const auto bad_index =
        corrupt->materialize(root_lower, root_upper, lower_out, upper_out, scratch, enough);
    require(bad_index.status == MaterializationStatus::invalid_dimensions,
            "a delta addressing a missing variable fails closed");
    require(enough.charged() == 0, "an out-of-range delta charges nothing");
}

void test_materialization_bytes_and_scope_depth() {
    const std::size_t variables = 5;
    auto node = NodeView::root();
    Contribution contribution;
    contribution.tightened_upper = std::make_pair<std::size_t, Bound>(0, Bound::finite(1.0));
    for (int depth = 0; depth < 3; ++depth) node = node->child(contribution);
    require(node->depth() == 3, "depth accumulates over the chain");
    require(node->bound_delta_count() == 3, "each step adds one delta");
    require(node->materialization_bytes(variables) == 2U * variables * sizeof(Bound),
            "materialization bytes match the two output vectors");
}
} // namespace

int main() {
    test_root_view();
    test_child_shares_and_records();
    test_sibling_cut_scope_is_structural();
    test_basis_and_evidence_inheritance();
    test_materialization_matches_full_reconstruction();
    test_materialization_preflight_failures();
    test_materialization_bytes_and_scope_depth();
    return 0;
}
