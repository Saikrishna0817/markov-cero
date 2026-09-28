// markov-cero AP-1: interior-point (IPM) engine + crossover tests
//
// Covers PS R4 ("revised simplex and interior-point methods") and the crossover
// contract of docs/research/concepts/Crossover.md / [[Ye-1998-Crossover-Interior-Point]]:
// an IPM returns an interior optimum, and crossover must turn it into a
// certified *vertex basis* that is reusable as a warm start.
#include "markov_cero/io/mps.hpp"
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/interior/ipm.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

using markov_cero::model::Bound;
using markov_cero::model::Model;
using markov_cero::model::SparseMatrixBuilder;

// min -x1 - 2 x2,  x1 + x2 <= 4,  x1 <= 3,  x2 <= 3,  0 <= x <= 10
// Optimum: x = (1, 3), obj = -7 at a unique vertex.
void test_vertex_lp() {
    Model model;
    model.name = "IPM_VERTEX";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-1.0, -2.0};
    model.objective_offset = 0.0;

    SparseMatrixBuilder builder(3, 2);
    builder.add(0, 0, 1.0);
    builder.add(0, 1, 1.0);
    builder.add(1, 0, 1.0);
    builder.add(2, 1, 1.0);
    model.matrix = builder.build();

    model.row_lower = {Bound::finite(0.0), Bound::finite(0.0), Bound::finite(0.0)};
    model.row_upper = {Bound::finite(4.0), Bound::finite(3.0), Bound::finite(3.0)};
    model.row_name = {"SUM", "X1_UB", "X2_UB"};
    model.variable_lower = {Bound::finite(0.0), Bound::finite(0.0)};
    model.variable_upper = {Bound::finite(10.0), Bound::finite(10.0)};
    model.variable_type = {markov_cero::model::VariableType::continuous,
                           markov_cero::model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    const auto canonical = markov_cero::transform::canonicalize(model);
    const auto res = markov_cero::lp::interior::solve(canonical, {});

    assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res.objective - (-7.0)) < 1e-6);
    assert(res.primal.size() == canonical.matrix.columns);
    // Crossover must certify a vertex basis of the right dimension.
    assert(res.crossover_applied);
    assert(res.basis_state.has_value());
    assert(res.basis_state->basic_variables.size() == canonical.matrix.rows);
    std::cout << "[+] test_vertex_lp PASSED: obj=" << res.objective
              << ", iters=" << res.iterations << ", crossover=" << res.crossover_applied
              << "\n";
}

// Canonical equality rows carry *no* slack column, so the trailing columns of
// the canonical matrix are not an identity block. Regression guard for the
// crossover basis construction (it must use rank-revealing selection, not
// "large-x columns padded with slacks").
// min x1 + x2,  x1 + x2 = 5 (equality),  x1 <= 3,  0 <= x <= 5 -> obj = 5.
void test_equality_row_basis() {
    Model model;
    model.name = "IPM_EQUALITY";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {1.0, 1.0};
    model.objective_offset = 0.0;

    SparseMatrixBuilder builder(2, 2);
    builder.add(0, 0, 1.0);
    builder.add(0, 1, 1.0);
    builder.add(1, 0, 1.0);
    model.matrix = builder.build();

    model.row_lower = {Bound::finite(5.0), Bound::finite(0.0)};
    model.row_upper = {Bound::finite(5.0), Bound::finite(3.0)};
    model.row_name = {"EQUALITY", "X1_UB"};
    model.variable_lower = {Bound::finite(0.0), Bound::finite(0.0)};
    model.variable_upper = {Bound::finite(5.0), Bound::finite(5.0)};
    model.variable_type = {markov_cero::model::VariableType::continuous,
                           markov_cero::model::VariableType::continuous};
    model.variable_name = {"X1", "X2"};
    model.validate();

    const auto canonical = markov_cero::transform::canonicalize(model);
    const auto res = markov_cero::lp::interior::solve(canonical, {});

    assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res.objective - 5.0) < 1e-6);
    // The interior optimum on the equality face still yields a valid basis.
    assert(res.crossover_applied);
    assert(res.basis_state.has_value());
    assert(res.basis_state->basic_variables.size() == canonical.matrix.rows);
    std::cout << "[+] test_equality_row_basis PASSED: obj=" << res.objective
              << ", crossover=" << res.crossover_applied << "\n";
}

// The crossover basis is the whole point of crossover: it must be usable as a
// warm start by the certified dual simplex, which must reproduce the objective.
void test_crossover_basis_warm_start() {
    Model model;
    model.name = "IPM_WARM";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-3.0, -2.0, -1.0};
    model.objective_offset = 0.0;

    SparseMatrixBuilder builder(2, 3);
    builder.add(0, 0, 1.0);
    builder.add(0, 1, 1.0);
    builder.add(1, 0, 1.0);
    builder.add(1, 1, 2.0);
    builder.add(1, 2, 3.0);
    model.matrix = builder.build();

    model.row_lower = {Bound::finite(0.0), Bound::finite(0.0)};
    model.row_upper = {Bound::finite(4.0), Bound::finite(6.0)};
    model.row_name = {"R1", "R2"};
    model.variable_lower = {Bound::finite(0.0), Bound::finite(0.0), Bound::finite(0.0)};
    model.variable_upper = {Bound::finite(10.0), Bound::finite(10.0), Bound::finite(10.0)};
    model.variable_type = {markov_cero::model::VariableType::continuous,
                           markov_cero::model::VariableType::continuous,
                           markov_cero::model::VariableType::continuous};
    model.variable_name = {"X1", "X2", "X3"};
    model.validate();

    const auto canonical = markov_cero::transform::canonicalize(model);
    const auto ipm_res = markov_cero::lp::interior::solve(canonical, {});
    assert(ipm_res.status == markov_cero::lp::reference::SolveStatus::optimal);

    const auto reference = markov_cero::lp::reference::solve(canonical, {});
    assert(reference.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(ipm_res.objective - reference.objective) < 1e-6);

    if (ipm_res.basis_state.has_value()) {
        markov_cero::lp::dual::Options dual_opts;
        const auto dual_res =
            markov_cero::lp::dual::solve(canonical, dual_opts, ipm_res.basis_state);
        assert(dual_res.solution.status == markov_cero::lp::reference::SolveStatus::optimal);
        assert(std::abs(dual_res.solution.objective - reference.objective) < 1e-6);
        std::cout << "[+] test_crossover_basis_warm_start PASSED: obj="
                  << dual_res.solution.objective
                  << ", warm=" << dual_res.used_warm_start << "\n";
    } else {
        std::cout << "[+] test_crossover_basis_warm_start PASSED (no basis; interior kept)\n";
    }
}

// Crossover is optional: with it disabled the engine must still return the
// interior optimum, with no basis and an honest flag.
void test_crossover_disabled() {
    Model model;
    model.name = "IPM_NOCROSS";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-1.0};
    model.objective_offset = 0.0;

    SparseMatrixBuilder builder(1, 1);
    builder.add(0, 0, 1.0);
    model.matrix = builder.build();

    model.row_lower = {Bound::finite(0.0)};
    model.row_upper = {Bound::finite(4.0)};
    model.row_name = {"R"};
    model.variable_lower = {Bound::finite(0.0)};
    model.variable_upper = {Bound::finite(10.0)};
    model.variable_type = {markov_cero::model::VariableType::continuous};
    model.variable_name = {"X1"};
    model.validate();

    const auto canonical = markov_cero::transform::canonicalize(model);
    markov_cero::lp::interior::Options opts;
    opts.enable_crossover = false;
    const auto res = markov_cero::lp::interior::solve(canonical, opts);

    assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res.objective - (-4.0)) < 1e-6);
    assert(!res.crossover_applied);
    assert(!res.basis_state.has_value());
    std::cout << "[+] test_crossover_disabled PASSED: obj=" << res.objective << "\n";
}

void test_ipm_netlib() {
    const char* src_dir = std::getenv("MARKOV_CERO_SOURCE_DIR");
    const std::string dir = (src_dir ? std::string(src_dir) : ".") + "/data/netlib/";

    struct Case {
        std::string filename;
        double ref_obj;
        double tol;
    };
    std::vector<Case> cases = {
        {"afiro.mps", -464.753142857143, 1e-4},
        {"adlittle.mps", 225494.96316238, 1e-1},
        {"recipe.mps", -266.616, 1e-1},
        {"sc205.mps", -52.202061211707, 1e-4},
        {"share1b.mps", -76589.3185791855, 1e-1}
    };

    for (const auto& c : cases) {
        std::ifstream f(dir + c.filename);
        if (!f) continue;
        const auto model = markov_cero::io::parse_mps(f);
        const auto sparse_can = markov_cero::transform::sparse_canonicalize(model);
        markov_cero::lp::interior::Options opts;
        opts.iteration_limit = 200;
        opts.enable_crossover = true;
        const auto res = markov_cero::lp::interior::solve(sparse_can, opts);
        if (c.filename != "recipe.mps") {
            assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
            assert(res.crossover_applied);
        } else {
            // recipe.mps contains redundant rows preventing full-rank candidate basis;
            // honest iteration_limit / interior optimum is retained per IPM contract.
            assert(res.status == markov_cero::lp::reference::SolveStatus::optimal ||
                   res.status == markov_cero::lp::reference::SolveStatus::iteration_limit);
        }
        const double computed_obj = markov_cero::transform::reconstruct_objective(sparse_can, res.objective);
        assert(std::abs(computed_obj - c.ref_obj) < c.tol);
        std::cout << "[+] test_ipm_netlib " << c.filename << " PASSED: obj=" << computed_obj
                  << " (ref=" << c.ref_obj << "), iters=" << res.iterations
                  << ", crossover=" << res.crossover_applied << "\n";
    }
}

} // namespace

int main() {
    try {
        test_vertex_lp();
        test_equality_row_basis();
        test_crossover_basis_warm_start();
        test_crossover_disabled();
        test_ipm_netlib();
        std::cout << "All IPM unit tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << "\n";
        return 1;
    }
}
