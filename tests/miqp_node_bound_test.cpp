// MIQP-01 contract §7.3/§7.4 (docs/contracts/miqp-node-bounds.md): node
// bound overlays reach both the QP and the support minimization, an
// infinite box endpoint stays inconclusive through full search, and the
// blueprint §13 required MIQP cases hold.

#include "markov_cero/api/solve.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/milp/node_lp.hpp"
#include "markov_cero/model/model.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace markov_cero;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
bool contains(const std::string& text, const char* fragment) {
    return text.find(fragment) != std::string::npos;
}

model::Model build(const std::string& name, model::ObjectiveSense sense, double offset,
                   const std::vector<double>& q, const std::vector<double>& quad_diag,
                   const std::vector<model::Bound>& lo, const std::vector<model::Bound>& hi,
                   const std::vector<model::VariableType>& types,
                   const std::vector<std::vector<double>>& rows,
                   const std::vector<model::Bound>& row_lo,
                   const std::vector<model::Bound>& row_hi) {
    const std::size_t n = q.size();
    model::Model out;
    out.name = name;
    out.objective_sense = sense;
    out.objective_offset = offset;
    out.objective = q;
    out.variable_lower = lo;
    out.variable_upper = hi;
    out.variable_type = types;
    for (std::size_t j = 0; j < n; ++j) out.variable_name.push_back("X" + std::to_string(j));
    model::SparseMatrixBuilder ab(rows.size(), n);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        out.row_lower.push_back(row_lo[i]);
        out.row_upper.push_back(row_hi[i]);
        out.row_name.push_back("R" + std::to_string(i));
        for (std::size_t j = 0; j < n; ++j)
            if (rows[i][j] != 0.0) ab.add(i, j, rows[i][j]);
    }
    out.matrix = ab.build();
    if (!quad_diag.empty()) {
        out.has_quadratic_objective = true;
        model::SparseMatrixBuilder qb(n, n);
        for (std::size_t j = 0; j < n; ++j)
            if (quad_diag[j] != 0.0) qb.add(j, j, quad_diag[j]);
        out.quadratic_matrix = qb.build();
    }
    out.validate();
    return out;
}

api::SolveOptions miqp_options() {
    api::SolveOptions options;
    options.engine = "miqp";
    options.milp_options.time_limit_seconds = 60.0;
    return options;
}

// §7.3: lower- and upper-side node overlays reach the QP primal and the
// support minimization (scenario_9 covers the lower side of f = x²−4x;
// both directions are asserted together here).
void test_node_overlay_reaches_qp_and_support() {
    const auto m = build("OVERLAY", model::ObjectiveSense::minimize, 0.0, {-4.0}, {2.0},
                         {model::Bound::finite(0.0)}, {model::Bound::finite(10.0)},
                         {model::VariableType::integer}, {}, {}, {});
    const auto lower_side = milp::solve_node_qp(m, {}, {model::Bound::finite(3.0)},
                                                {model::Bound::finite(10.0)});
    require(lower_side.status == lp::reference::SolveStatus::optimal, "lower overlay solves");
    require(std::fabs(lower_side.primal[0] - 3.0) < 1e-3, "lower overlay reaches the QP");
    require(lower_side.lower_bound <= lower_side.objective + 1e-7 &&
                lower_side.lower_bound > lower_side.objective - 1e-3,
            "lower overlay reaches the support minimization");
    const auto upper_side = milp::solve_node_qp(m, {}, {model::Bound::finite(0.0)},
                                                {model::Bound::finite(1.0)});
    require(upper_side.status == lp::reference::SolveStatus::optimal, "upper overlay solves");
    require(std::fabs(upper_side.primal[0] - 1.0) < 1e-3, "upper overlay reaches the QP");
    require(std::fabs(upper_side.objective + 3.0) < 1e-3, "upper overlay shifts the node optimum");
    require(upper_side.lower_bound <= upper_side.objective + 1e-7 &&
                upper_side.lower_bound > upper_side.objective - 1e-3,
            "upper overlay reaches the support minimization");
    std::cout << "[+] node overlays reach both the QP and the support minimization\n";
}

// §7.3 + §13: x has NO box; rows 0 ≤ x ≤ 5 bound the region linearly. No
// finite box support exists, so the node fails closed to unsupported and
// the full search stops inconclusive — never optimal, never a fabricated
// bound, proven tier blocked. (P5 retry-then-unresolved keeps the root's
// unsupported status; the failure_site label defect is contract §6.3.)
void test_unbounded_box_stays_inconclusive() {
    const auto m = build("UNBOUNDED_BOX", model::ObjectiveSense::minimize, 4.41, {-4.2}, {2.0},
                         {model::Bound::negative_infinity()},
                         {model::Bound::positive_infinity()},
                         {model::VariableType::integer}, {{1.0}},
                         {model::Bound::finite(0.0)}, {model::Bound::finite(5.0)});
    const auto node = milp::solve_node_qp(m, {});
    require(node.status == lp::reference::SolveStatus::unsupported,
            "infinite box endpoint fails closed at the node");
    require(contains(node.message, "infinite box infimum"), "node message names the cause");
    require(node.lower_bound == -std::numeric_limits<double>::infinity(),
            "no bound fabricated at the node");
    const auto result = api::solve_model(m, miqp_options());
    require(result.status == lp::reference::SolveStatus::unsupported,
            "root failure surfaces unsupported, never optimal");
    require(contains(result.message, "infinite box infimum"), "search message preserves the cause");
    require(!std::isfinite(result.best_bound), "no fabricated global bound");
    require(result.assurance != "proven", "proven tier blocked without a finite support");
    std::cout << "[+] unbounded-box region stays inconclusive with no fabricated bound\n";
}

// §13 case 1: binary y, min(y−0.3)² — relaxation optimum 0, integer optimum
// 0.09; both children materialize with their overlaid [0,0] / [1,1] boxes
// (full-solve child materialization, §7.3).
void test_half_squared_binary_bounds() {
    const auto m = build("HALF_SQ", model::ObjectiveSense::minimize, 0.09, {-0.6}, {2.0},
                         {model::Bound::finite(0.0)}, {model::Bound::finite(1.0)},
                         {model::VariableType::integer}, {}, {}, {});
    const auto root = milp::solve_node_qp(m, {});
    require(root.status == lp::reference::SolveStatus::optimal, "root relaxation solves");
    require(std::fabs(root.objective) < 1e-3, "relaxation optimum 0 at y = 0.3");
    require(root.lower_bound <= 1e-9, "root bound ≤ 0, never the integer value");
    const auto child0 = milp::solve_node_qp(m, {}, {model::Bound::finite(0.0)},
                                            {model::Bound::finite(0.0)});
    require(child0.status == lp::reference::SolveStatus::optimal && std::fabs(child0.objective - 0.09) < 1e-4,
            "child y ≤ 0 has objective 0.09");
    require(child0.lower_bound <= child0.objective + 1e-7 &&
                child0.lower_bound > child0.objective - 1e-3,
            "child y ≤ 0 bound uses the [0,0] overlay");
    const auto child1 = milp::solve_node_qp(m, {}, {model::Bound::finite(1.0)},
                                            {model::Bound::finite(1.0)});
    require(child1.status == lp::reference::SolveStatus::optimal && std::fabs(child1.objective - 0.49) < 1e-4,
            "child y ≥ 1 has objective 0.49");
    require(child1.lower_bound <= child1.objective + 1e-7 &&
                child1.lower_bound > child1.objective - 1e-3,
            "child y ≥ 1 bound uses the [1,1] overlay");
    const auto result = api::solve_model(m, miqp_options());
    require(result.status == lp::reference::SolveStatus::optimal, "binary MIQP solves");
    require(std::fabs(result.objective - 0.09) < 1e-6, "integer optimum 0.09 at y = 0");
    require(result.primal.size() == 1 && std::fabs(result.primal[0]) < 1e-4, "argmin y = 0");
    require(result.original_verified, "incumbent verified from original quadratic data");
    std::cout << "[+] min(y−0.3)²: root/child bounds and integer optimum 0.09\n";
}

// §13 case 2: min(x−1)²+0.2y with 0 ≤ x ≤ 2, y ∈ {0,1}, x ≤ y → (1,1), 0.2.
void test_linear_coupling_optimum() {
    const auto m = build("COUPLE", model::ObjectiveSense::minimize, 1.0, {-2.0, 0.2},
                         {2.0, 0.0}, {model::Bound::finite(0.0), model::Bound::finite(0.0)},
                         {model::Bound::finite(2.0), model::Bound::finite(1.0)},
                         {model::VariableType::integer, model::VariableType::integer},
                         {{1.0, -1.0}}, {model::Bound::negative_infinity()},
                         {model::Bound::finite(0.0)});
    const auto root = milp::solve_node_qp(m, {});
    require(root.status == lp::reference::SolveStatus::optimal, "root relaxation solves");
    require(root.lower_bound <= 0.2 + 1e-6, "root bound ≤ relaxation optimum 0.2");
    const auto result = api::solve_model(m, miqp_options());
    require(result.status == lp::reference::SolveStatus::optimal, "coupled MIQP solves");
    require(std::fabs(result.objective - 0.2) < 1e-6, "optimum is 0.2");
    require(result.primal.size() == 2 && std::fabs(result.primal[0] - 1.0) < 1e-4 &&
                std::fabs(result.primal[1] - 1.0) < 1e-4,
            "argmin is (1,1)");
    require(result.original_verified, "incumbent verified from original quadratic data");
    std::cout << "[+] min(x−1)²+0.2y with x ≤ y: optimum 0.2 at (1,1)\n";
}

// §13 case 4: indefinite and nearly-PSD Hessians never yield optimal —
// the ADMM convexity gate rejects both before any bound exists (contract §3).
void test_indefinite_hessian_never_optimal() {
    const auto concave = build("CONCAVE", model::ObjectiveSense::minimize, 0.0, {0.0}, {-2.0},
                               {model::Bound::finite(0.0)}, {model::Bound::finite(1.0)},
                               {model::VariableType::integer}, {}, {}, {});
    const auto node = milp::solve_node_qp(concave, {});
    require(node.status == lp::reference::SolveStatus::numerical_failure,
            "non-convex Hessian rejected as numerical_failure, never optimal");
    const auto nearly = build("NEARLY_PSD", model::ObjectiveSense::minimize, 0.0, {0.0},
                              {-1e-9}, {model::Bound::finite(0.0)},
                              {model::Bound::finite(1.0)}, {model::VariableType::integer},
                              {}, {}, {});
    const auto node2 = milp::solve_node_qp(nearly, {});
    require(node2.status == lp::reference::SolveStatus::numerical_failure,
            "nearly-PSD Hessian rejected beyond the 1e-10 tolerance");
    const auto result = api::solve_model(concave, miqp_options());
    require(result.status != lp::reference::SolveStatus::optimal,
            "indefinite full solve never reports optimal");
    std::cout << "[+] indefinite and nearly-PSD Hessians never yield optimal\n";
}

// §13 case 5: maximize with a nonzero offset — objective and global bound
// reverse into the original max space (max −(x−1)²+5 = −x²+2x+4).
void test_maximize_offset_sign_reversal() {
    const auto m = build("MAX_OFFSET", model::ObjectiveSense::maximize, 4.0, {2.0}, {-2.0},
                         {model::Bound::finite(0.0)}, {model::Bound::finite(2.0)},
                         {model::VariableType::integer}, {}, {}, {});
    const auto result = api::solve_model(m, miqp_options());
    require(result.status == lp::reference::SolveStatus::optimal, "maximize MIQP solves");
    require(std::fabs(result.objective - 5.0) < 1e-6, "objective reported in original max space");
    require(result.primal.size() == 1 && std::fabs(result.primal[0] - 1.0) < 1e-4, "argmax x = 1");
    require(std::isfinite(result.best_bound), "global bound finite after sign reversal");
    require(result.best_bound > 4.9, "bound reversed to the original max space near +5");
    require(std::fabs(result.best_bound - result.objective) < 1e-5,
            "bound closes the gap on the correct side");
    require(result.original_verified, "incumbent verified from original quadratic data");
    std::cout << "[+] maximize + nonzero offset reverses objective and bound signs\n";
}
} // namespace

int main() {
    try {
        test_node_overlay_reaches_qp_and_support();
        test_unbounded_box_stays_inconclusive();
        test_half_squared_binary_bounds();
        test_linear_coupling_optimum();
        test_indefinite_hessian_never_optimal();
        test_maximize_offset_sign_reversal();
        std::cout << "All MIQP node-bound tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[-] Error: " << error.what() << "\n";
        return 1;
    }
}
