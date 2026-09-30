#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/milp/cuts.hpp"
#include "markov_cero/milp/node_propagation.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include "support/tiny_exact.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_singleton_propagation_evidence() {
    using namespace markov_cero;
    model::Model model;
    model::SparseMatrixBuilder builder(1, 1);
    builder.add(0, 0, 2.0);
    model.matrix = builder.build();
    model.objective = {1.0};
    model.row_lower = {model::Bound::finite(4.0)};
    model.row_upper = {model::Bound::positive_infinity()};
    model.variable_lower = {model::Bound::finite(0.0)};
    model.variable_upper = {model::Bound::finite(5.0)};
    model.variable_type = {model::VariableType::integer};
    auto lower = model.variable_lower, upper = model.variable_upper;
    milp::NodeBounds overlay;
    const auto propagated = milp::propagate_singleton_rows(model, lower, upper, overlay);
    require(propagated.tightened && lower[0].value == 2.0, "singleton row tightens domain");
    require(propagated.evidence.source == milp::BoundEvidenceSource::propagated &&
            propagated.evidence.value == 2.0, "propagated objective evidence");
    milp::NodeView::Contribution contribution;
    contribution.tightened_lower = {0, lower[0]};
    contribution.lower_bound = propagated.evidence;
    const auto view = milp::NodeView::root()->child(contribution);
    milp::NodeBounds::MaterializationScratch scratch;
    core::MemoryBudget budget;
    std::vector<model::Bound> materialized_lower, materialized_upper;
    const auto materialized = view->materialize(model.variable_lower, model.variable_upper,
        materialized_lower, materialized_upper, scratch, budget);
    require(materialized.ok() && materialized_lower[0].value == 2.0 &&
            view->lower_bound_evidence().source == milp::BoundEvidenceSource::propagated,
            "materialized node retains propagated evidence");
    milp::NodeView::release(materialized, budget);
}

void test_serial_singleton_propagation_parity() {
    using namespace markov_cero;
    model::Model implied;
    model::SparseMatrixBuilder builder(2, 3);
    builder.add(0, 0, 2.0);
    builder.add(1, 1, 3.0);
    builder.add(1, 2, 3.0);
    implied.matrix = builder.build();
    implied.objective = {0.0, -1.0, -1.0};
    implied.row_lower = {model::Bound::finite(2.0), model::Bound::negative_infinity()};
    implied.row_upper = {model::Bound::positive_infinity(), model::Bound::finite(5.0)};
    implied.variable_lower.assign(3, model::Bound::finite(0.0));
    implied.variable_upper.assign(3, model::Bound::finite(1.0));
    implied.variable_type.assign(3, model::VariableType::binary);
    implied.row_name = {"FIX", "PACK"};
    implied.variable_name = {"X0", "X1", "X2"};
    implied.validate();
    auto explicit_bound = implied;
    explicit_bound.variable_lower[0] = model::Bound::finite(1.0);
    milp::Options options;
    options.enable_cuts = false;
    options.enable_heuristics = false;
    options.enable_strong_branching = false;
    const auto propagated = milp::solve(implied, options);
    const auto baseline = milp::solve(explicit_bound, options);
    require(propagated.status == baseline.status &&
            propagated.status == lp::reference::SolveStatus::optimal &&
            std::abs(propagated.objective - baseline.objective) < 1e-8,
            "singleton propagation preserves MILP status and objective");
    bool emitted = false;
    for (const auto& note : propagated.obligations)
        emitted |= note.kind == verify::MipObligationKind::propagation &&
                   note.source_row == 0 && note.variable == 0;
    require(emitted, "node relaxation emits singleton propagation obligation");
}

void test_gomory_cut_generation() {
    markov_cero::model::Model model;
    model.name = "CUT_TEST";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-10.0, -14.0, -12.0};

    markov_cero::model::SparseMatrixBuilder builder(1, 3);
    builder.add(0, 0, 4.0);
    builder.add(0, 1, 6.0);
    builder.add(0, 2, 5.0);
    model.matrix = builder.build();

    model.row_lower = {markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::finite(10.0)};
    model.row_name = {"CAPACITY"};

    model.variable_lower = {markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0)};
    model.variable_type = {markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary};
    model.variable_name = {"X1", "X2", "X3"};
    model.validate();

    // Solve continuous LP relaxation
    const auto canon =
        markov_cero::transform::sparse_canonicalize(model, /*relax_integrality=*/true);
    const auto dense = canon.to_dense();
    const auto lpres = markov_cero::lp::reference::solve(dense);
    assert(lpres.status == markov_cero::lp::reference::SolveStatus::optimal);

    const auto primal = markov_cero::transform::reconstruct_primal(canon, lpres.primal);
    const auto basis_state = markov_cero::lp::dual::make_basis_state(dense, lpres.basis);

    const auto cuts = markov_cero::milp::generate_gomory_cuts(model, primal, canon, basis_state, 5);
    for (const auto& cut : cuts) {
        assert(cut.violation > 0.0); // Strictly cuts off fractional LP point
        // Contract §7.5: enumerate every integer-feasible point of the box
        // (4 x1 + 6 x2 + 5 x3 <= 10) and require the row to keep all of them.
        markov_cero::test_support::enumerate_integer_box(
            {0, 0, 0}, {1, 1, 1}, [&](const std::vector<int>& p) {
                if (4.0 * p[0] + 6.0 * p[1] + 5.0 * p[2] > 10.0 + 1e-9) return;
                const double lhs = cut.coefficients[0] * p[0] +
                                   cut.coefficients[1] * p[1] + cut.coefficients[2] * p[2];
                assert(lhs >= cut.rhs - 1e-6);
            });
    }

    auto augmented_model = model;
    markov_cero::milp::add_cuts_to_model(augmented_model, cuts);
    assert(augmented_model.matrix.row_count == model.matrix.row_count + cuts.size());
    std::cout << "[+] test_gomory_cut_generation passed (" << cuts.size() << " cuts generated)\n";
    markov_cero::milp::Options options;
    options.enable_heuristics = false;
    options.enable_strong_branching = false;
    const auto solved = markov_cero::milp::solve(model, options);
    require(solved.cuts_generated > 0, "solver applies cuts during search");
    std::size_t cut_notes = 0;
    for (const auto& note : solved.obligations) {
        if (note.kind != markov_cero::verify::MipObligationKind::cut) continue;
        ++cut_notes;
        // Every applied cut's obligation must be a finite row violated at the
        // pre-cut separation point it was recorded against (contract §5.4).
        require(std::isfinite(note.rhs) && std::isfinite(note.observed_lhs) &&
                    note.observed_lhs < note.rhs,
                "cut obligation is a violated finite row at the pre-cut point");
    }
    require(cut_notes == solved.cuts_generated,
            "every applied serial cut emits an audit obligation");
}

void test_cover_cut_generation() {
    markov_cero::model::Model model;
    model.name = "COVER_TEST";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-1.0, -1.0, -1.0};

    markov_cero::model::SparseMatrixBuilder builder(1, 3);
    // 3 x1 + 3 x2 + 3 x3 <= 5
    builder.add(0, 0, 3.0);
    builder.add(0, 1, 3.0);
    builder.add(0, 2, 3.0);
    model.matrix = builder.build();

    model.row_lower = {markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::finite(5.0)};
    model.row_name = {"KNAPSACK"};

    model.variable_lower = {markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0)};
    model.variable_type = {markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary};
    model.variable_name = {"X1", "X2", "X3"};
    model.validate();

    // Fractional point: x1 = 0.8333, x2 = 0.8333, x3 = 0.0 (3*0.8333 + 3*0.8333 = 5.0)
    std::vector<double> frac_primal = {5.0 / 6.0, 5.0 / 6.0, 0.0};
    const auto cuts = markov_cero::milp::generate_cover_cuts(model, frac_primal, 5);
    assert(!cuts.empty());
    for (const auto& cut : cuts) {
        assert(cut.violation > 0.0);
        // Contract §7.5: enumerate every integer point of the box and require
        // the row to keep all that satisfy 3 x1 + 3 x2 + 3 x3 <= 5.
        markov_cero::test_support::enumerate_integer_box(
            {0, 0, 0}, {1, 1, 1}, [&](const std::vector<int>& p) {
                if (3.0 * p[0] + 3.0 * p[1] + 3.0 * p[2] > 5.0 + 1e-9) return;
                const double lhs = cut.coefficients[0] * p[0] +
                                   cut.coefficients[1] * p[1] + cut.coefficients[2] * p[2];
                assert(lhs >= cut.rhs - 1e-6);
            });
    }
    std::cout << "[+] test_cover_cut_generation passed (" << cuts.size() << " cover cuts generated)\n";
}

} // namespace

int main() {
    try {
        test_gomory_cut_generation();
        test_cover_cut_generation();
        test_singleton_propagation_evidence();
        test_serial_singleton_propagation_parity();
        std::cout << "All cuts tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << "\n";
        return 1;
    }
}
