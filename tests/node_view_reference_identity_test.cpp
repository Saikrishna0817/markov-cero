#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/milp/node_lp.hpp"
#include "markov_cero/milp/reference_materialisation.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using markov_cero::model::Bound;
using markov_cero::model::Model;
using markov_cero::milp::NodeBounds;
using markov_cero::milp::ReferenceStep;
using markov_cero::milp::compare_bounds_to_reference;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// 4-binary knapsack: minimize -5x1 -4x2 -3x3 -2x4 s.t. 2x1 + 4x2 + 3x3 + x4 <= 7.
Model make_knapsack() {
    Model model;
    model.name = "REFERENCE_IDENTITY";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-5.0, -4.0, -3.0, -2.0};
    model.objective_offset = 0.0;
    markov_cero::model::SparseMatrixBuilder builder(1, 4);
    builder.add(0, 0, 2.0);
    builder.add(0, 1, 4.0);
    builder.add(0, 2, 3.0);
    builder.add(0, 3, 1.0);
    model.matrix = builder.build();
    model.row_lower = {markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {Bound::finite(7.0)};
    model.row_name = {"CAPACITY"};
    model.variable_lower.assign(4, Bound::finite(0.0));
    model.variable_upper.assign(4, Bound::finite(1.0));
    model.variable_type.assign(4, markov_cero::model::VariableType::binary);
    model.variable_name = {"X1", "X2", "X3", "X4"};
    model.validate();
    return model;
}

std::pair<std::vector<Bound>, std::vector<Bound>> root_bounds(std::size_t variables) {
    return {std::vector<Bound>(variables, Bound::finite(0.0)),
            std::vector<Bound>(variables, Bound::finite(1.0))};
}

// Acceptance: identical original-space certificates. The node LP is solved
// twice — once from the production delta chain, once from the flat reference
// replay — and both must return the same certificate, which still verifies
// against the original model rows and bounds.
void test_node_lp_identity() {
    const Model model = make_knapsack();
    const auto [root_lower, root_upper] = root_bounds(4);

    // Down-branch on x2 through two rewrites of the same upper bound: the
    // final value (0) must win, which makes chain order observable in the
    // vectors the node LP receives.
    NodeBounds chain;
    chain = chain.with_upper(1, Bound::finite(0.5));
    chain = chain.with_upper(1, Bound::finite(0.0));
    std::vector<ReferenceStep> steps;
    steps.push_back(ReferenceStep{1, std::nullopt, Bound::finite(0.5), {}});
    steps.push_back(ReferenceStep{1, std::nullopt, Bound::finite(0.0), {}});

    const auto compared = compare_bounds_to_reference(chain, root_lower, root_upper, steps);
    require(compared.ok(), "reference replay must match the production chain");

    std::vector<Bound> production_lower = root_lower;
    std::vector<Bound> production_upper = root_upper;
    chain.materialize(root_lower, root_upper, production_lower, production_upper);
    std::vector<Bound> reference_lower = root_lower;
    std::vector<Bound> reference_upper = root_upper;
    for (const auto& step : steps) {
        if (step.lower) reference_lower[step.variable] = *step.lower;
        if (step.upper) reference_upper[step.variable] = *step.upper;
    }
    require(production_lower.size() == reference_lower.size() &&
                production_upper.size() == reference_upper.size(),
            "materialized dimensions agree");
    for (std::size_t i = 0; i < 4; ++i) {
        require(production_lower[i].value == reference_lower[i].value, "lower vector identity");
        require(production_upper[i].value == reference_upper[i].value, "upper vector identity");
    }

    const markov_cero::milp::Options options;
    const auto via_production = markov_cero::milp::solve_node_relaxation(
        model, options, std::nullopt, production_lower, production_upper);
    const auto via_reference = markov_cero::milp::solve_node_relaxation(
        model, options, std::nullopt, reference_lower, reference_upper);
    require(via_production.status == via_reference.status, "node LP status identity");
    require(via_production.objective == via_reference.objective, "node LP objective identity");
    require(via_production.primal.size() == via_reference.primal.size(), "node LP primal shape");
    for (std::size_t i = 0; i < via_production.primal.size(); ++i)
        require(via_production.primal[i] == via_reference.primal[i], "node LP primal identity");
    require(via_production.status == markov_cero::lp::reference::SolveStatus::optimal,
            "node LP solves to optimality");
    require(via_production.primal[1] == 0.0, "tightened bound honored in original space");

    const markov_cero::verify::Candidate candidate{via_production.primal,
                                                   via_production.objective};
    const auto report = markov_cero::verify::verify_primal(
        model, candidate, {}, {}, 1e-6, /*require_integrality=*/false);
    require(report.passed, "node LP certificate verifies against the original model");
}

// Acceptance: identical original-space solutions. The full solve through the
// persistent delta representation returns a certificate that verifies against
// the untouched original model.
void test_full_solve_original_space() {
    const Model model = make_knapsack();
    markov_cero::milp::Options options;
    options.max_nodes = 5000;
    const auto result = markov_cero::milp::solve(model, options);
    require(result.status == markov_cero::lp::reference::SolveStatus::optimal,
            "small knapsack proves optimal");
    require(result.nodes_explored >= 1, "search explores at least one node");
    const markov_cero::verify::Candidate candidate{result.primal, result.objective};
    const auto report = markov_cero::verify::verify_primal(model, candidate);
    require(report.passed, "full-solve solution verifies in original space");
}
} // namespace

int main() {
    try {
        test_node_lp_identity();
        test_full_solve_original_space();
        std::cout << "[+] node_view_reference_identity_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[-] node_view_reference_identity_test: " << error.what() << '\n';
        return 1;
    }
}
