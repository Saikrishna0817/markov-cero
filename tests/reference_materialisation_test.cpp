#include "markov_cero/milp/reference_materialisation.hpp"

#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using markov_cero::model::Bound;
using markov_cero::milp::NodeBounds;
using markov_cero::milp::NodeView;
using markov_cero::milp::ReferenceCaps;
using markov_cero::milp::ReferenceComparison;
using markov_cero::milp::ReferenceFacet;
using markov_cero::milp::ReferenceStatus;
using markov_cero::milp::ReferenceStep;
using markov_cero::milp::compare_bounds_to_reference;
using markov_cero::milp::compare_view_to_reference;
using Contribution = NodeView::Contribution;
using CutId = NodeView::CutId;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::pair<std::vector<Bound>, std::vector<Bound>> root_bounds(std::size_t variables) {
    std::vector<Bound> lower(variables, Bound::finite(0.0));
    std::vector<Bound> upper(variables, Bound::finite(1.0));
    return {lower, upper};
}

std::size_t expected_delta_count(const std::vector<ReferenceStep>& steps) {
    std::size_t count = 0;
    for (const auto& step : steps)
        count += static_cast<std::size_t>(step.lower.has_value()) +
                 static_cast<std::size_t>(step.upper.has_value());
    return count;
}

NodeBounds build_bounds(const std::vector<ReferenceStep>& steps) {
    NodeBounds chain;
    for (const auto& step : steps) {
        if (step.lower) chain = chain.with_lower(step.variable, *step.lower);
        if (step.upper) chain = chain.with_upper(step.variable, *step.upper);
    }
    return chain;
}

Contribution contribution_of(const ReferenceStep& step) {
    Contribution contribution;
    if (step.lower) contribution.tightened_lower = {step.variable, *step.lower};
    if (step.upper) contribution.tightened_upper = {step.variable, *step.upper};
    contribution.local_cuts = step.local_cuts;
    return contribution;
}

std::shared_ptr<const NodeView> build_view(const std::shared_ptr<const NodeView>& root,
                                            const std::vector<ReferenceStep>& steps) {
    auto view = root;
    for (const auto& step : steps) view = view->child(contribution_of(step));
    return view;
}

ReferenceStep random_step(std::mt19937& rng, std::size_t variables) {
    ReferenceStep step;
    step.variable = std::uniform_int_distribution<std::size_t>(0, variables - 1)(rng);
    std::uniform_int_distribution<int> coin(0, 3);
    if (coin(rng) % 2 != 0)
        step.lower =
            Bound::finite(std::uniform_real_distribution<double>(0.0, 0.5)(rng));
    if (coin(rng) % 2 != 0)
        step.upper =
            Bound::finite(std::uniform_real_distribution<double>(0.5, 1.0)(rng));
    if (coin(rng) == 0) step.local_cuts = {static_cast<CutId>(100 + step.variable)};
    return step;
}

// The oracle must hold on randomized chains: repeated writes to the same
// variable across steps make chain order observable, and a view built through
// child() must agree with a flat chronological replay of the same script.
void test_randomized_chains() {
    std::mt19937 rng(20260928U);
    for (int trial = 0; trial < 200; ++trial) {
        const std::size_t variables =
            std::uniform_int_distribution<std::size_t>(1, 12)(rng);
        const std::size_t depth = std::uniform_int_distribution<std::size_t>(1, 10)(rng);
        std::vector<ReferenceStep> steps;
        for (std::size_t d = 0; d < depth; ++d) steps.push_back(random_step(rng, variables));
        const auto [lower, upper] = root_bounds(variables);
        const NodeBounds chain = build_bounds(steps);
        const auto view = build_view(NodeView::root(), steps);
        require(chain.delta_count() == expected_delta_count(steps), "delta count mismatch");
        require(view->bound_delta_count() == chain.delta_count(), "view/bounds delta drift");
        const ReferenceComparison bounds =
            compare_bounds_to_reference(chain, lower, upper, steps);
        require(bounds.ok(), "randomized bounds comparison diverged");
        const ReferenceComparison view_cmp = compare_view_to_reference(*view, lower, upper, steps);
        require(view_cmp.ok(), "randomized view comparison diverged");
    }
}

// Acceptance: randomized push/pop matches full reconstruction. Push appends a
// step and its child; pop drops back to the retained parent view, which must
// still equal a fresh chronological replay of the prefix script.
void test_push_pop_reversibility() {
    std::mt19937 rng(91U);
    const auto [lower, upper] = root_bounds(6);
    const auto root = NodeView::root();
    std::vector<std::shared_ptr<const NodeView>> views{root};
    std::vector<NodeBounds> chains{NodeBounds{}};
    std::vector<ReferenceStep> script;
    for (int op = 0; op < 400; ++op) {
        const bool push = script.empty() ||
                          (std::uniform_int_distribution<int>(0, 1)(rng) != 0 &&
                           script.size() < 12);
        if (push) {
            const ReferenceStep step = random_step(rng, 6);
            script.push_back(step);
            views.push_back(views.back()->child(contribution_of(step)));
            NodeBounds next = chains.back();
            if (step.lower) next = next.with_lower(step.variable, *step.lower);
            if (step.upper) next = next.with_upper(step.variable, *step.upper);
            chains.push_back(next);
        } else {
            script.pop_back();
            views.pop_back();
            chains.pop_back();
        }
        require(compare_view_to_reference(*views.back(), lower, upper, script).ok(),
                "push/pop view diverged from reconstruction");
        require(compare_bounds_to_reference(chains.back(), lower, upper, script).ok(),
                "push/pop bounds diverged from reconstruction");
    }
}

// Acceptance: sibling branch mutation isolation. Children of one parent carry
// only their own deltas and cuts; the parent and each sibling keep matching
// their own scripts after every child is created.
void test_sibling_isolation() {
    const auto [lower, upper] = root_bounds(8);
    std::vector<ReferenceStep> parent_script;
    parent_script.push_back(ReferenceStep{3, Bound::finite(0.25), std::nullopt, {}});
    const auto parent = build_view(NodeView::root(), parent_script);
    const NodeBounds parent_chain = build_bounds(parent_script);

    ReferenceStep down_step;
    down_step.variable = 3;
    down_step.upper = Bound::finite(0.5);
    ReferenceStep up_step;
    up_step.variable = 6;
    up_step.lower = Bound::finite(0.4);
    up_step.local_cuts = {42};
    const auto down = parent->child(contribution_of(down_step));
    const auto up = parent->child(contribution_of(up_step));

    std::vector<ReferenceStep> down_script = parent_script;
    down_script.push_back(down_step);
    std::vector<ReferenceStep> up_script = parent_script;
    up_script.push_back(up_step);
    require(compare_view_to_reference(*down, lower, upper, down_script).ok(),
            "down child diverged from its script");
    require(compare_view_to_reference(*up, lower, upper, up_script).ok(),
            "up child diverged from its script");
    require(compare_view_to_reference(*parent, lower, upper, parent_script).ok(),
            "parent mutated after branching");
    require(compare_bounds_to_reference(parent_chain, lower, upper, parent_script).ok(),
            "parent bounds mutated after branching");
    require(!down->cut_in_scope(42), "sibling cut leaked into down child");
    require(up->cut_in_scope(42), "own cut missing from up child");
    require(!parent->cut_in_scope(42), "child cut leaked into parent");
}

// Refusal happens before allocation: an over-cap request never reports bytes.
void test_caps_refuse_before_allocation() {
    const auto [lower, upper] = root_bounds(32);
    std::vector<ReferenceStep> steps;
    for (std::size_t i = 0; i < 4; ++i) {
        ReferenceStep step;
        step.variable = i;
        step.lower = Bound::finite(0.1);
        step.local_cuts = {static_cast<CutId>(i)};
        steps.push_back(step);
    }
    // Five recorded cut ids across four steps: the cut total exceeds any cap
    // that still admits the step count itself.
    steps[0].local_cuts.push_back(1001);
    const NodeBounds chain = build_bounds(steps);
    ReferenceCaps too_few_variables;
    too_few_variables.max_variables = 31;
    ReferenceComparison capped =
        compare_bounds_to_reference(chain, lower, upper, steps, too_few_variables);
    require(capped.status == ReferenceStatus::refused, "variable cap must refuse");
    require(capped.reference_bytes == 0, "refusal must allocate nothing");
    ReferenceCaps too_few_steps;
    too_few_steps.max_steps = 3;
    capped = compare_bounds_to_reference(chain, lower, upper, steps, too_few_steps);
    require(capped.status == ReferenceStatus::refused, "step cap must refuse");
    require(capped.reference_bytes == 0, "step refusal must allocate nothing");
    ReferenceCaps too_few_bytes;
    too_few_bytes.max_bytes = 2U * 32U * sizeof(Bound) - 1U;
    capped = compare_bounds_to_reference(chain, lower, upper, steps, too_few_bytes);
    require(capped.status == ReferenceStatus::refused, "byte cap must refuse");
    require(capped.reference_bytes == 0, "byte refusal must allocate nothing");
    ReferenceCaps too_few_cuts;
    too_few_cuts.max_steps = 4;
    const auto view = build_view(NodeView::root(), steps);
    const ReferenceComparison view_capped =
        compare_view_to_reference(*view, lower, upper, steps, too_few_cuts);
    require(view_capped.status == ReferenceStatus::refused, "cut total must refuse");
    require(view_capped.reference_bytes == 0, "cut refusal must allocate nothing");
}

void test_mismatch_reporting() {
    const auto [lower, upper] = root_bounds(8);
    std::vector<ReferenceStep> script;
    script.push_back(ReferenceStep{1, Bound::finite(0.3), std::nullopt, {}});
    // Production carries an extra tightening the script never recorded.
    NodeBounds production = build_bounds(script).with_lower(5, Bound::finite(0.25));
    ReferenceComparison cmp = compare_bounds_to_reference(production, lower, upper, script);
    require(cmp.status == ReferenceStatus::mismatch, "extra delta must be detected");
    require(cmp.facet == ReferenceFacet::lower_bounds, "extra lower delta facet");
    require(cmp.first_mismatch_index == 5, "extra delta reports its variable");
    // A wrong value at a known index reports that exact index.
    std::vector<ReferenceStep> wrong = script;
    wrong[0].lower = Bound::finite(0.4);
    cmp = compare_bounds_to_reference(build_bounds(script), lower, upper, wrong);
    require(cmp.status == ReferenceStatus::mismatch &&
                cmp.facet == ReferenceFacet::lower_bounds && cmp.first_mismatch_index == 1,
            "wrong value reports its index");
    // Upper-surface divergence reports the upper facet.
    NodeBounds upper_only = build_bounds(script).with_upper(4, Bound::finite(0.5));
    cmp = compare_bounds_to_reference(upper_only, lower, upper, script);
    require(cmp.status == ReferenceStatus::mismatch && cmp.facet == ReferenceFacet::upper_bounds &&
                cmp.first_mismatch_index == 4,
            "upper divergence reports upper facet");
    // Cut-scope divergence reports its position in the scoped list.
    std::vector<ReferenceStep> cut_script;
    cut_script.push_back(ReferenceStep{0, std::nullopt, std::nullopt, {7, 9}});
    const auto view = build_view(NodeView::root(), cut_script);
    std::vector<ReferenceStep> wrong_cuts = cut_script;
    wrong_cuts[0].local_cuts = {7, 11};
    cmp = compare_view_to_reference(*view, lower, upper, wrong_cuts);
    require(cmp.status == ReferenceStatus::mismatch && cmp.facet == ReferenceFacet::cut_scope &&
                cmp.first_mismatch_index == 1,
            "cut divergence reports its position");
}

void test_invalid_inputs() {
    const auto [lower, upper] = root_bounds(4);
    const std::vector<Bound> short_upper(3, Bound::finite(1.0));
    std::vector<ReferenceStep> steps;
    steps.push_back(ReferenceStep{0, Bound::finite(0.5), std::nullopt, {}});
    ReferenceComparison cmp = compare_bounds_to_reference(NodeBounds{}, lower, short_upper, steps);
    require(cmp.status == ReferenceStatus::invalid, "dimension mismatch is invalid");
    require(cmp.reference_bytes == 0, "invalid inputs allocate nothing");
    steps[0].variable = 4;
    cmp = compare_bounds_to_reference(build_bounds(steps), lower, upper, steps);
    require(cmp.status == ReferenceStatus::invalid, "out-of-range step is invalid");
}
} // namespace

int main() {
    try {
        test_randomized_chains();
        test_push_pop_reversibility();
        test_sibling_isolation();
        test_caps_refuse_before_allocation();
        test_mismatch_reporting();
        test_invalid_inputs();
        std::cout << "[+] reference_materialisation_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[-] reference_materialisation_test: " << error.what() << '\n';
        return 1;
    }
}
