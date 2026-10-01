// MIQP-01 contract §7.5 (docs/contracts/miqp-node-bounds.md): MIQP
// incumbents come from the original quadratic data. A true incumbent passes
// the engine gate (verify_primal) and proof replay; a tampered quadratic
// objective or a tampered primal is rejected at both — including the
// maximize-space sign path.

#include "markov_cero/api/solve.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/verify/mip_proof.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include <cmath>
#include <iostream>
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

// min (x−1)² over x ∈ {0,1,2} — quadratic, integer, optimum 0 at x = 1.
model::Model square_choice(model::ObjectiveSense sense, double offset, double q_coef,
                           double quad_entry) {
    model::Model m;
    m.name = "SQUARE_CHOICE";
    m.objective_sense = sense;
    m.objective_offset = offset;
    m.objective = {q_coef};
    m.has_quadratic_objective = true;
    m.quadratic_matrix.row_count = 1;
    m.quadratic_matrix.column_count = 1;
    m.quadratic_matrix.column_start = {0, 1};
    m.quadratic_matrix.row_index = {0};
    m.quadratic_matrix.value = {quad_entry};
    m.matrix = model::SparseMatrixBuilder(0, 1).build();
    m.variable_lower = {model::Bound::finite(0.0)};
    m.variable_upper = {model::Bound::finite(2.0)};
    m.variable_type = {model::VariableType::integer};
    m.variable_name = {"X"};
    m.validate();
    return m;
}

// Engine gate (engine_milp.cpp:71-84 semantics): verify_primal recomputes
// the quadratic objective from the original model, so only the honest
// incumbent passes.
void test_engine_gate_rejects_tampered_incumbent() {
    const auto m = square_choice(model::ObjectiveSense::minimize, 1.0, -2.0, 2.0);
    require(verify::verify_primal(m, {{1.0}, 0.0}).passed, "true incumbent passes the gate");
    const auto inflated = verify::verify_primal(m, {{1.0}, 0.25});
    require(!inflated.passed, "inflated quadratic objective rejected at the engine gate");
    require(!inflated.violations.empty() && inflated.violations[0].category == "objective",
            "rejection is attributed to the objective");
    require(!verify::verify_primal(m, {{1.0}, -0.25}).passed,
            "deflated quadratic objective rejected at the engine gate");
    require(!verify::verify_primal(m, {{0.4}, 0.36}).passed,
            "non-integer incumbent rejected even with a matching objective");
    require(!verify::verify_primal(m, {{3.0}, 4.0}).passed,
            "out-of-box incumbent rejected");
    std::cout << "[+] engine gate rejects tampered quadratic incumbents\n";
}

// §7.5 first clause: a real MIQP solve asserts original_verified.
void test_full_solve_marks_original_verified() {
    const auto m = square_choice(model::ObjectiveSense::minimize, 1.0, -2.0, 2.0);
    api::SolveOptions options;
    options.engine = "miqp";
    options.milp_options.time_limit_seconds = 60.0;
    const auto result = api::solve_model(m, options);
    require(result.status == lp::reference::SolveStatus::optimal, "MIQP solve is optimal");
    require(result.original_verified, "incumbent verified from original quadratic data");
    require(std::fabs(result.objective) < 1e-6, "objective 0 at x = 1");
    std::cout << "[+] full MIQP solve reports original_verified\n";
}

// Replay gate (mip_proof.cpp:61-64): the recorded incumbent is rechecked
// against the original model before any bound or gap gate runs.
void test_replay_rejects_tampered_incumbent() {
    const auto m = square_choice(model::ObjectiveSense::minimize, 1.0, -2.0, 2.0);
    const auto proof = verify::build_mip_proof(m, {1.0}, 0.0, false);
    require(verify::verify_mip_proof(m, proof).accepted, "true quadratic proof accepted");
    auto bad_objective = proof;
    bad_objective.objective = 0.5;
    require(!verify::verify_mip_proof(m, bad_objective).accepted,
            "tampered proof objective rejected at replay");
    auto bad_value = proof;
    bad_value.objective = -0.5;
    require(!verify::verify_mip_proof(m, bad_value).accepted,
            "below-optimum proof objective rejected at replay");
    auto bad_primal = proof;
    bad_primal.incumbent = {0.5};
    require(!verify::verify_mip_proof(m, bad_primal).accepted,
            "tampered incumbent primal rejected at replay");
    auto bad_point = proof;
    bad_point.incumbent = {3.0};
    require(!verify::verify_mip_proof(m, bad_point).accepted,
            "out-of-box incumbent rejected at replay");
    std::cout << "[+] replay rechecks the incumbent: tampered records rejected\n";
}

// Same replay gate on a maximize model — the incumbent rides in original
// max space and must still be rechecked against the original quadratic.
void test_replay_rejects_tampered_maximize_incumbent() {
    const auto m = square_choice(model::ObjectiveSense::maximize, 4.0, 2.0, -2.0);
    const auto proof = verify::build_mip_proof(m, {1.0}, 5.0, false);
    require(verify::verify_mip_proof(m, proof).accepted, "maximize quadratic proof accepted");
    auto tampered = proof;
    tampered.objective = 5.5;
    require(!verify::verify_mip_proof(m, tampered).accepted,
            "tampered maximize objective rejected at replay");
    tampered = proof;
    tampered.objective = 4.5;
    require(!verify::verify_mip_proof(m, tampered).accepted,
            "under-stated maximize objective rejected at replay");
    std::cout << "[+] replay rechecks maximize-space incumbents against the original model\n";
}
} // namespace

int main() {
    try {
        test_engine_gate_rejects_tampered_incumbent();
        test_full_solve_marks_original_verified();
        test_replay_rejects_tampered_incumbent();
        test_replay_rejects_tampered_maximize_incumbent();
        std::cout << "All MIQP incumbent tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[-] Error: " << error.what() << "\n";
        return 1;
    }
}
