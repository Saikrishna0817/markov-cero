// MIQP-01 contract §7.6 (docs/contracts/miqp-node-bounds.md): proof attacks
// on quadratic trees. The control proof carries a fractional relaxation
// (bound leaf), a QP Farkas leaf for the infeasible child and a split node.
// Every tampered variant must be rejected — zero accepted false proofs.

#include "markov_cero/model/model.hpp"
#include "markov_cero/verify/mip_proof.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace markov_cero;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// min (x−1)² with x integer in [0,2] and row x ≥ 1.2: the relaxation optimum
// x = 1.2 is fractional, so the proof needs a split; the x ≤ 1 child is
// infeasible (QP Farkas leaf), the x ≥ 2 child carries the incumbent.
model::Model bound_row_quadratic() {
    model::Model m;
    m.name = "BOUND_ROW_QUAD";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective_offset = 1.0;
    m.objective = {-2.0};
    m.has_quadratic_objective = true;
    m.quadratic_matrix.row_count = 1;
    m.quadratic_matrix.column_count = 1;
    m.quadratic_matrix.column_start = {0, 1};
    m.quadratic_matrix.row_index = {0};
    m.quadratic_matrix.value = {2.0};
    model::SparseMatrixBuilder ab(1, 1);
    ab.add(0, 0, 1.0);
    m.matrix = ab.build();
    m.row_lower = {model::Bound::finite(1.2)};
    m.row_upper = {model::Bound::positive_infinity()};
    m.row_name = {"LOWER"};
    m.variable_lower = {model::Bound::finite(0.0)};
    m.variable_upper = {model::Bound::finite(2.0)};
    m.variable_type = {model::VariableType::integer};
    m.variable_name = {"X"};
    m.validate();
    return m;
}

std::size_t first_of(const verify::MipProof& proof, verify::MipProofKind kind) {
    for (std::size_t i = 0; i < proof.nodes.size(); ++i)
        if (proof.nodes[i].kind == kind) return i;
    throw std::runtime_error("control proof is missing a required node kind");
}

void rejected(const model::Model& m, const verify::MipProof& proof, const std::string& what) {
    if (verify::verify_mip_proof(m, proof).accepted)
        throw std::runtime_error("accepted false proof: " + what);
}

void run_attacks() {
    const auto m = bound_row_quadratic();
    const auto control = verify::build_mip_proof(m, {2.0}, 1.0, false);
    require(verify::verify_mip_proof(m, control).accepted, "control quadratic proof accepted");
    require(control.nodes.size() >= 3, "control proof has split and leaf nodes");
    const auto bound_i = first_of(control, verify::MipProofKind::bound);
    const auto infeasible_i = first_of(control, verify::MipProofKind::infeasible);
    const auto split_i = first_of(control, verify::MipProofKind::split);

    auto attack = control;
    attack.nodes[bound_i].relaxation.primal = {1.4};
    rejected(m, attack, "altered QP primal");

    attack = control;
    attack.nodes[bound_i].relaxation.dual = {-0.9};
    rejected(m, attack, "altered QP row multiplier");

    attack = control;
    attack.nodes[bound_i].relaxation.objective += 0.5;
    rejected(m, attack, "altered QP objective");

    attack = control;
    attack.nodes[infeasible_i].relaxation.certificate = {0.4};
    rejected(m, attack, "forged QP Farkas certificate");

    attack = control;
    attack.nodes[bound_i].relaxation.status = lp::reference::SolveStatus::infeasible;
    rejected(m, attack, "bound leaf relabeled infeasible");

    attack = control;
    attack.nodes[infeasible_i].kind = verify::MipProofKind::bound;
    rejected(m, attack, "Farkas leaf relabeled bound");

    attack = control;
    attack.nodes[split_i].split_value = 1.5;
    rejected(m, attack, "non-integer split value");

    attack = control;
    std::swap(attack.nodes[split_i].down, attack.nodes[split_i].up);
    rejected(m, attack, "swapped split children");

    attack = control;
    attack.nodes[split_i].split_value = 0;
    rejected(m, attack, "split value forged against recorded witnesses");

    attack = control;
    attack.claims_infeasible = true;
    rejected(m, attack, "claims_infeasible against a feasible QP tree");

    attack = control;
    attack.incumbent = {7.0};
    rejected(m, attack, "forged incumbent point");

    attack = control;
    attack.objective = 0.5;
    rejected(m, attack, "forged incumbent objective");

    require(verify::verify_mip_proof(m, control).accepted,
            "untampered control still accepted after the attack sweep");
    std::cout << "[+] all quadratic-tree proof attacks rejected (zero accepted false proofs)\n";
}
} // namespace

int main() {
    try {
        run_attacks();
        std::cout << "All MIQP proof-attack tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[-] Error: " << error.what() << "\n";
        return 1;
    }
}
