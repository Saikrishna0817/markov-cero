#include "markov_cero/io/mps.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/presolve/presolve.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace {

constexpr const char* blend_mps = R"(NAME BLEND
OBJSENSE
 MIN
ROWS
 N COST
 E TOTAL
 L SULFUR
COLUMNS
 A COST 40 TOTAL 1
 A SULFUR 0.01
 B COST 30 TOTAL 1
 B SULFUR 0.03
RHS
 RHS1 TOTAL 100 SULFUR 2
BOUNDS
 LO BND A 0
 LO BND B 0
ENDATA
)";

void test_presolve_empty_row_infeasible() {
    markov_cero::transform::SparseCanonicalModel model;
    model.matrix.rows = 1;
    model.matrix.columns = 1;
    model.matrix.column_offsets = {0, 0};
    model.rhs = {10.0}; // 0*x = 10 -> infeasible
    model.objective = {1.0};
    model.record.objective_sign = 1.0;

    const auto res = markov_cero::presolve::presolve(model);
    assert(res.status == markov_cero::lp::reference::SolveStatus::infeasible);
}

void test_presolve_empty_column_unbounded() {
    markov_cero::transform::SparseCanonicalModel model;
    model.matrix.rows = 1;
    model.matrix.columns = 1;
    model.matrix.column_offsets = {0, 0};
    model.rhs = {0.0};
    model.objective = {-5.0}; // empty col with negative cost -> unbounded
    model.record.objective_sign = 1.0;

    const auto res = markov_cero::presolve::presolve(model);
    assert(res.status == markov_cero::lp::reference::SolveStatus::unbounded);
}

void test_presolve_row_singleton_reduction() {
    // 2 rows, 2 cols:
    // row 0: 2 * x0 = 6  (row singleton -> fixes x0 = 3)
    // row 1: x0 + x1 = 7 (incident row -> becomes x1 = 4)
    markov_cero::transform::SparseCanonicalModel model;
    model.matrix.rows = 2;
    model.matrix.columns = 2;
    model.matrix.column_offsets = {0, 2, 3};
    model.matrix.row_indices = {0, 1, 1};
    model.matrix.values = {2.0, 1.0, 1.0};
    model.rhs = {6.0, 7.0};
    model.objective = {10.0, 5.0};
    model.record.objective_sign = 1.0;

    const auto presolved = markov_cero::presolve::presolve(model);
    assert(presolved.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(presolved.statistics.row_singletons_removed >= 1);

    // After row singletons and fixed variable elimination, model is fully reduced
    markov_cero::lp::reference::Result reduced_sol;
    reduced_sol.status = markov_cero::lp::reference::SolveStatus::optimal;
    reduced_sol.primal.assign(presolved.model.matrix.columns, 0.0);
    reduced_sol.dual.assign(presolved.model.matrix.rows, 0.0);

    const auto restored = markov_cero::presolve::postsolve(presolved.stack, reduced_sol, model);
    assert(std::abs(restored.primal[0] - 3.0) < 1e-9);
    assert(std::abs(restored.primal[1] - 4.0) < 1e-9);
    // Objective: 10*3 + 5*4 = 50
    assert(std::abs(restored.objective - 50.0) < 1e-9);
}

void test_blend_with_presolve_matches_without_presolve() {
    const auto mps = markov_cero::io::parse_mps_string(blend_mps);
    const auto sparse = markov_cero::transform::sparse_canonicalize(mps);

    // Solve without presolve
    const auto dense = sparse.to_dense();
    const auto sol_unpresolved = markov_cero::lp::reference::solve(dense);
    assert(sol_unpresolved.status == markov_cero::lp::reference::SolveStatus::optimal);

    // Solve with presolve
    const auto presolved = markov_cero::presolve::presolve(sparse);
    assert(presolved.status == markov_cero::lp::reference::SolveStatus::optimal);

    markov_cero::lp::reference::Result sol_reduced;
    if (presolved.model.matrix.rows > 0 && presolved.model.matrix.columns > 0) {
        sol_reduced = markov_cero::lp::reference::solve(presolved.model.to_dense());
        assert(sol_reduced.status == markov_cero::lp::reference::SolveStatus::optimal);
    } else {
        sol_reduced.status = markov_cero::lp::reference::SolveStatus::optimal;
    }

    const auto sol_postsolved =
        markov_cero::presolve::postsolve(presolved.stack, sol_reduced, sparse);
    assert(std::abs(sol_postsolved.objective - sol_unpresolved.objective) < 1e-7);

    // Verify primal variables reconstructed
    auto prim_orig_presolved =
        markov_cero::transform::reconstruct_primal(sparse, sol_postsolved.primal);
    auto prim_orig_unpresolved =
        markov_cero::transform::reconstruct_primal(sparse, sol_unpresolved.primal);
    for (std::size_t j = 0; j < prim_orig_presolved.size(); ++j) {
        assert(std::abs(prim_orig_presolved[j] - prim_orig_unpresolved[j]) < 1e-6);
    }
}

// AP-9: forcing row — all-positive coefficients with rhs 0 must fix every
// incident variable at 0 and keep the postsolved solution optimal with duals.
void test_presolve_forcing_row() {
    markov_cero::transform::SparseCanonicalModel model;
    model.matrix.rows = 2;
    model.matrix.columns = 3;
    model.matrix.column_offsets = {0, 2, 4, 5};
    // Row 0 (forcing): x0 + x1 = 0. Row 1: x0 + x1 + x2 = 4.
    model.matrix.row_indices = {0, 1, 0, 1, 1};
    model.matrix.values = {1.0, 1.0, 1.0, 1.0, 1.0};
    model.rhs = {0.0, 4.0};
    model.objective = {2.0, 3.0, 1.0};
    model.record.objective_sign = 1.0;

    const auto presolved = markov_cero::presolve::presolve(model);
    assert(presolved.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(presolved.statistics.forcing_rows_removed == 1);
    // Forcing row fixes x0 and x1 at 0; multi-pass presolve then reduces the
    // second row to the singleton x2 = 4 and fixes x2 as well, so no columns
    // survive (x0 + x1 = 0, x0 + x1 + x2 = 4 imply x2 = 4 exactly).
    assert(presolved.statistics.fixed_vars_removed == 3);
    assert(presolved.model.matrix.columns == 0);

    auto sol = markov_cero::lp::reference::solve(presolved.model.to_dense());
    assert(sol.status == markov_cero::lp::reference::SolveStatus::optimal);
    const auto restored = markov_cero::presolve::postsolve(presolved.stack, sol, model);
    assert(std::abs(restored.objective - 4.0) < 1e-7);  // x2 = 4 at cost 1
    assert(std::abs(restored.primal[0]) < 1e-9 && std::abs(restored.primal[1]) < 1e-9);
    // Dual of the forcing row must keep every incident reduced cost
    // dual-feasible: pi_0 <= (c_j - sum_{r != 0} a_rj pi_r) / a_0j. For j = 0 the
    // other active row contributes a_10*pi_1 = 1, so pi_0 = (2 - 1)/1 = 1 and not
    // the naive c_0/a_00 = 2 (which would make rc_0 = -1 < 0).
    assert(std::abs(restored.dual[0] - 1.0) < 1e-7);
    // Full-witness dual feasibility: rc_j = c_j - A^T pi >= 0 for every column,
    // including the ones the forcing row fixed at zero.
    for (std::size_t j = 0; j < model.matrix.columns; ++j) {
        double aty = 0.0;
        for (std::size_t p = model.matrix.column_offsets[j];
             p < model.matrix.column_offsets[j + 1]; ++p) {
            aty += model.matrix.values[p] * restored.dual[model.matrix.row_indices[p]];
        }
        assert(model.objective[j] - aty >= -1e-9);
    }
}

// AP-9: duplicate rows — the second copy is redundant and must be dropped with
// identical objective after postsolve.
void test_presolve_duplicate_rows() {
    markov_cero::transform::SparseCanonicalModel model;
    model.matrix.rows = 2;
    model.matrix.columns = 2;
    model.matrix.column_offsets = {0, 2, 4};
    model.matrix.row_indices = {0, 1, 0, 1};
    model.matrix.values = {1.0, 1.0, 1.0, 1.0};
    model.rhs = {4.0, 4.0};  // identical constraint twice
    model.objective = {1.0, 2.0};
    model.record.objective_sign = 1.0;

    const auto presolved = markov_cero::presolve::presolve(model);
    assert(presolved.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(presolved.statistics.duplicate_rows_removed == 1);
    // Multi-pass: after the duplicate row drops, the equal-pattern columns
    // are proportional with different cost (AP-9 dominated drop), the
    // remaining row becomes a singleton that fixes x0 = 4, and the emptied
    // row is removed — no active rows/columns survive; postsolve restores
    // the full witness.
    assert(presolved.model.matrix.rows == 0);
    assert(presolved.model.matrix.columns == 0);

    auto sol = markov_cero::lp::reference::solve(presolved.model.to_dense());
    assert(sol.status == markov_cero::lp::reference::SolveStatus::optimal);
    const auto restored = markov_cero::presolve::postsolve(presolved.stack, sol, model);
    assert(std::abs(restored.objective - 4.0) < 1e-7);  // x0 = 4 at cost 1
    // Primal feasibility of the dropped row must still hold.
    const double dropped_row_activity = restored.primal[0] + restored.primal[1];
    assert(std::abs(dropped_row_activity - 4.0) < 1e-7);
}

// AP-9: dominated duplicate columns — proportional columns (s = 1/2), the
// worse-cost twin (per unit of x_a) must be fixed at zero.
void test_presolve_dominated_columns() {
    markov_cero::transform::SparseCanonicalModel model;
    model.matrix.rows = 1;
    model.matrix.columns = 2;
    model.matrix.column_offsets = {0, 1, 2};
    model.matrix.row_indices = {0, 0};
    model.matrix.values = {2.0, 1.0};  // col1 = (1/2) * col0
    model.rhs = {4.0};
    model.objective = {2.0, 3.0};  // per unit of column activity: 2/2 = 1 < 3/1
    model.record.objective_sign = 1.0;

    const auto presolved = markov_cero::presolve::presolve(model);
    assert(presolved.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(presolved.statistics.dominated_cols_removed == 1);
    // Multi-pass: after the worse twin drops, the row is a singleton that
    // fixes x0 = 4/2 = 2, so no columns (or rows) survive; postsolve
    // restores x = (2, 0) with the original objective.
    assert(presolved.model.matrix.columns == 0);

    auto sol = markov_cero::lp::reference::solve(presolved.model.to_dense());
    assert(sol.status == markov_cero::lp::reference::SolveStatus::optimal);
    const auto restored = markov_cero::presolve::postsolve(presolved.stack, sol, model);
    assert(std::abs(restored.objective - 4.0) < 1e-7);  // x0 = 2, cost 2 each
    assert(std::abs(restored.primal[1]) < 1e-9);
}

} // namespace

int main() {
    try {
        test_presolve_empty_row_infeasible();
        test_presolve_empty_column_unbounded();
        test_presolve_row_singleton_reduction();
        test_blend_with_presolve_matches_without_presolve();
        test_presolve_forcing_row();
        test_presolve_duplicate_rows();
        test_presolve_dominated_columns();
        std::cout << "presolve tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
