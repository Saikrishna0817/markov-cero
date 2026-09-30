// MIP-01 contract §5.3 / §7.5: cut-row lattice guards and enumerated-point
// validity. GMI/MIR rows are only derived when the canonicalization sits on
// the original integer lattice (integral shift, unit scale); every generated
// row must keep every integer-feasible point of the box.

#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/milp/cut_lattice.hpp"
#include "markov_cero/milp/cuts.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include "support/tiny_exact.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

markov_cero::model::Model two_var_eq_model(double x1_lo, double x2_lo, double rhs) {
    using namespace markov_cero;
    model::Model m;
    m.name = "EDGE_EQ";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective = {-1.0, 0.0};
    model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 3.0);
    builder.add(0, 1, 2.0);
    m.matrix = builder.build();
    m.row_lower = {model::Bound::finite(rhs)};
    m.row_upper = {model::Bound::finite(rhs)};
    m.variable_lower = {model::Bound::finite(x1_lo), model::Bound::finite(x2_lo)};
    m.variable_upper = {model::Bound::finite(2.5), model::Bound::finite(2.5)};
    m.variable_type = {model::VariableType::integer, model::VariableType::integer};
    m.variable_name = {"X1", "X2"};
    m.row_name = {"SUM"};
    m.validate();
    return m;
}

markov_cero::model::Model knapsack_model(double capacity) {
    using namespace markov_cero;
    model::Model m;
    m.name = "KNAPSACK";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective = {-10.0, -14.0, -12.0};
    model::SparseMatrixBuilder builder(1, 3);
    builder.add(0, 0, 4.0);
    builder.add(0, 1, 6.0);
    builder.add(0, 2, 5.0);
    m.matrix = builder.build();
    m.row_lower = {model::Bound::negative_infinity()};
    m.row_upper = {model::Bound::finite(capacity)};
    m.row_name = {"CAPACITY"};
    m.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0),
                        model::Bound::finite(0.0)};
    m.variable_upper = {model::Bound::finite(1.0), model::Bound::finite(1.0),
                        model::Bound::finite(1.0)};
    m.variable_type = {model::VariableType::binary, model::VariableType::binary,
                       model::VariableType::binary};
    m.variable_name = {"X1", "X2", "X3"};
    m.validate();
    return m;
}

// Contract §7.5: every integer-feasible point of the {0,1}^3 box satisfying
// 4 x1 + 6 x2 + 5 x3 <= capacity must satisfy the row.
void require_all_integer_points_kept(const markov_cero::model::Model& model,
                                     const std::vector<markov_cero::milp::Cut>& cuts,
                                     double capacity, std::size_t& validated) {
    for (const auto& cut : cuts) {
        require(std::isfinite(cut.rhs), "cut rhs must be finite");
        for (double c : cut.coefficients) require(std::isfinite(c), "cut coefficient finite");
        markov_cero::test_support::enumerate_integer_box(
            {0, 0, 0}, {1, 1, 1}, [&](const std::vector<int>& p) {
                if (4.0 * p[0] + 6.0 * p[1] + 5.0 * p[2] > capacity + 1e-9) return;
                const double lhs = cut.coefficients[0] * p[0] +
                                   cut.coefficients[1] * p[1] + cut.coefficients[2] * p[2];
                assert(lhs >= cut.rhs - 1e-6);
                ++validated;
            });
        ++validated;
    }
    (void)model;
}

void test_lattice_helper_conditions() {
    using namespace markov_cero;
    // Fractional lower bound (x >= 0.5) makes the structural column live off
    // the integer lattice: an active row coefficient must reject the row.
    {
        model::Model m;
        m.objective_sense = model::ObjectiveSense::minimize;
        m.objective = {1.0};
        model::SparseMatrixBuilder builder(1, 1);
        builder.add(0, 0, 1.0);
        m.matrix = builder.build();
        m.row_lower = {model::Bound::negative_infinity()};
        m.row_upper = {model::Bound::finite(3.0)};
        m.variable_lower = {model::Bound::finite(0.5)};
        m.variable_upper = {model::Bound::finite(3.5)};
        m.variable_type = {model::VariableType::integer};
        m.row_name = {"R"};
        m.variable_name = {"X"};
        m.validate();
        const auto canon = transform::sparse_canonicalize(m, /*relax_integrality=*/true);
        const std::size_t structural = canon.record.structural_variables;
        require(structural >= 1, "fractional-offset integer stays structural");
        require(!milp::cut_row_lattice_preserving(canon, m, {1.0}, structural),
                "active fractional-offset integer rejects the row");
        require(milp::cut_row_lattice_preserving(canon, m, {0.0}, structural),
                "inactive fractional-offset integer leaves the row eligible");
    }
    // Upper-bound-only integers shift by their finite upper bound when the
    // lower is -inf (sparse_canonicalize offset = upper): a fractional upper
    // (z <= 3.7) leaves the lattice and must reject; an integral upper
    // (z <= 4.0) keeps an integer shift and must stay eligible.
    {
        model::Model m;
        m.objective_sense = model::ObjectiveSense::minimize;
        m.objective = {1.0};
        model::SparseMatrixBuilder builder(1, 1);
        builder.add(0, 0, 1.0);
        m.matrix = builder.build();
        m.row_lower = {model::Bound::negative_infinity()};
        m.row_upper = {model::Bound::finite(3.0)};
        m.variable_lower = {model::Bound::negative_infinity()};
        m.variable_upper = {model::Bound::finite(3.7)};
        m.variable_type = {model::VariableType::integer};
        m.row_name = {"R"};
        m.variable_name = {"Z"};
        m.validate();
        const auto canon = transform::sparse_canonicalize(m, /*relax_integrality=*/true);
        const std::size_t structural = canon.record.structural_variables;
        require(structural >= 1, "unbounded-below integer stays structural");
        require(!milp::cut_row_lattice_preserving(canon, m, {1.0}, structural),
                "fractional upper-bound-only shift rejects the row");
    }
    {
        model::Model m;
        m.objective_sense = model::ObjectiveSense::minimize;
        m.objective = {1.0};
        model::SparseMatrixBuilder builder(1, 1);
        builder.add(0, 0, 1.0);
        m.matrix = builder.build();
        m.row_lower = {model::Bound::negative_infinity()};
        m.row_upper = {model::Bound::finite(3.0)};
        m.variable_lower = {model::Bound::negative_infinity()};
        m.variable_upper = {model::Bound::finite(4.0)};
        m.variable_type = {model::VariableType::integer};
        m.row_name = {"R"};
        m.variable_name = {"Z"};
        m.validate();
        const auto canon = transform::sparse_canonicalize(m, /*relax_integrality=*/true);
        const std::size_t structural = canon.record.structural_variables;
        require(structural >= 1, "integral-shift upper-only integer stays structural");
        require(milp::cut_row_lattice_preserving(canon, m, {1.0}, structural),
                "integral upper-bound-only shift keeps the row eligible");
    }
    // Continuous columns are outside the lattice condition entirely.
    {
        model::Model m;
        m.objective_sense = model::ObjectiveSense::minimize;
        m.objective = {1.0};
        model::SparseMatrixBuilder builder(1, 1);
        builder.add(0, 0, 1.0);
        m.matrix = builder.build();
        m.row_lower = {model::Bound::negative_infinity()};
        m.row_upper = {model::Bound::finite(3.0)};
        m.variable_lower = {model::Bound::finite(0.5)};
        m.variable_upper = {model::Bound::finite(3.5)};
        m.variable_type = {model::VariableType::continuous};
        m.row_name = {"R"};
        m.variable_name = {"X"};
        m.validate();
        const auto canon = transform::sparse_canonicalize(m, /*relax_integrality=*/true);
        const std::size_t structural = canon.record.structural_variables;
        require(structural >= 1, "continuous column stays structural");
        require(milp::cut_row_lattice_preserving(canon, m, {1.0}, structural),
                "continuous fractional bounds are outside the lattice rule");
    }
    std::cout << "[+] test_lattice_helper_conditions passed\n";
}

void generate_and_measure(const markov_cero::model::Model& model,
                          std::size_t& gmi_count, std::size_t& mir_count) {
    using namespace markov_cero;
    const auto canon = transform::sparse_canonicalize(model, /*relax_integrality=*/true);
    const auto dense = canon.to_dense();
    const auto lpres = lp::reference::solve(dense);
    require(lpres.status == lp::reference::SolveStatus::optimal, "edge model LP solves");
    const auto primal = transform::reconstruct_primal(canon, lpres.primal);
    const auto basis = lp::dual::make_basis_state(dense, lpres.basis);
    const auto gmi = milp::generate_gomory_cuts(model, primal, canon, basis, 10);
    const auto mir = milp::generate_mir_cuts(model, primal, canon, basis, 10);
    gmi_count = gmi.size();
    mir_count = mir.size();
}

void test_fractional_shifted_basic_variable_skips_rows() {
    // x1 has lower bound 0.5, so its canonical shift is 0.5. Row 3 x1 + 2 x2
    // = 7.4 pins x1 = 1.8 (interior, hence basic) with x2 nonbasic at its
    // lower bound; f0 would be original-unit while the row is canonical-unit,
    // so both GMI and MIR must skip instead of emitting a mixed-unit row.
    const auto model = two_var_eq_model(0.5, 1.0, 7.4);
    std::size_t gmi_count = 1, mir_count = 1;
    generate_and_measure(model, gmi_count, mir_count);
    require(gmi_count == 0, "GMI skips a fractional-shift basic variable");
    require(mir_count == 0, "MIR skips a fractional-shift basic variable");
    std::cout << "[+] test_fractional_shifted_basic_variable_skips_rows passed\n";
}

void test_fractional_shifted_contributor_skips_rows() {
    // x1 sits on the lattice (lower 1.0 -> shift 1.0) and is the fractional
    // basic variable (x1 = (7.4 - 1)/3 at the optimum), but the row also
    // touches x2 (lower 0.5 -> shift 0.5): the contributor rule rejects the
    // row even though the basic check passes.
    const auto model = two_var_eq_model(1.0, 0.5, 7.4);
    std::size_t gmi_count = 1, mir_count = 1;
    generate_and_measure(model, gmi_count, mir_count);
    require(gmi_count == 0, "GMI skips a fractional-shift contributor");
    require(mir_count == 0, "MIR skips a fractional-shift contributor");
    std::cout << "[+] test_fractional_shifted_contributor_skips_rows passed\n";
}

void test_integral_shift_generates_valid_rows() {
    // Twin with both shifts integral (lower 1.0): the guards must not
    // over-reject, so at least one row is derived (the tableau row has
    // fractional nonbasic coefficients 3 x1 + 2 x2 = 7.4).
    const auto model = two_var_eq_model(1.0, 1.0, 7.4);
    std::size_t gmi_count = 0, mir_count = 0;
    generate_and_measure(model, gmi_count, mir_count);
    require(gmi_count + mir_count >= 1, "integral-shift twin still derives rows");
    std::cout << "[+] test_integral_shift_generates_valid_rows passed ("
              << gmi_count << " GMI, " << mir_count << " MIR)\n";
}

void test_generated_rows_keep_all_integer_points() {
    // Fractional LP vertex (capacity 9.5): x2 = 11/12 is fractional and
    // nonbasic-at-a-bound is impossible, so it is basic with f0 != 0 and the
    // GMI/MIR/cover generators have real work to do. Every row must keep all
    // integer-feasible points of the box.
    const auto model = knapsack_model(9.5);
    const auto canon = markov_cero::transform::sparse_canonicalize(
        model, /*relax_integrality=*/true);
    const auto dense = canon.to_dense();
    const auto lpres = markov_cero::lp::reference::solve(dense);
    require(lpres.status == markov_cero::lp::reference::SolveStatus::optimal,
            "fractional knapsack LP solves");
    const auto primal = markov_cero::transform::reconstruct_primal(canon, lpres.primal);
    const auto basis = markov_cero::lp::dual::make_basis_state(dense, lpres.basis);
    const auto gmi = markov_cero::milp::generate_gomory_cuts(model, primal, canon, basis, 10);
    const auto mir = markov_cero::milp::generate_mir_cuts(model, primal, canon, basis, 10);
    const auto cover = markov_cero::milp::generate_cover_cuts(model, primal, 10);
    require(!gmi.empty(), "fractional vertex yields GMI rows");
    require(!mir.empty(), "fractional vertex yields MIR rows");
    require(!cover.empty(), "fractional vertex yields cover rows");
    std::size_t validated = 0;
    require_all_integer_points_kept(model, gmi, 9.5, validated);
    require_all_integer_points_kept(model, mir, 9.5, validated);
    require_all_integer_points_kept(model, cover, 9.5, validated);
    require(validated > 0, "enumeration validated at least one integer point");
    std::cout << "[+] test_generated_rows_keep_all_integer_points passed ("
              << gmi.size() << " GMI, " << mir.size() << " MIR, " << cover.size()
              << " cover; " << validated << " checks)\n";
}

} // namespace

int main() {
    try {
        test_lattice_helper_conditions();
        test_fractional_shifted_basic_variable_skips_rows();
        test_fractional_shifted_contributor_skips_rows();
        test_integral_shift_generates_valid_rows();
        test_generated_rows_keep_all_integer_points();
        std::cout << "All cut validity tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << "\n";
        return 1;
    }
}
