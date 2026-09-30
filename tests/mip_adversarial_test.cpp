// MIP-01 contract §7.1 / §7.4: adversarial proof suite. Overstated node
// bounds, out-of-domain/open leaves, structurally invalid obligations and
// false proofs rescued by cuts must all be rejected; production split
// children are cross-checked against the replay split rules.

#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/model/model_snapshot.hpp"
#include "markov_cero/verify/mip_proof.hpp"
#include "markov_cero/milp/branch_selector.hpp"
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../src/verify/mip_proof_internal.hpp"

using namespace markov_cero;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
bool contains(const std::string& text, const char* fragment) {
    return text.find(fragment) != std::string::npos;
}
} // namespace

int main() {
    auto model = io::parse_mps_string(
        "NAME ADVERSARIAL\nROWS\n N COST\n L CAP\nCOLUMNS\n X COST -1 CAP 1\n"
        "RHS\n R CAP 1.5\nBOUNDS\n LI B X 0\n UI B X 2\nENDATA\n");
    const auto proof = verify::build_mip_proof(model, {1}, -1, false);
    require(verify::verify_mip_proof(model, proof).accepted, "baseline proof accepted");

    // --- §7.1: corrupted node bounds (overstated bound claiming a false prune)
    const std::size_t bound_leaf = [&] {
        for (std::size_t i = 0; i < proof.nodes.size(); ++i)
            if (proof.nodes[i].kind == verify::MipProofKind::bound) return i;
        throw std::runtime_error("baseline proof has no bound leaf");
    }();
    {
        auto forged = proof;
        auto& leaf = forged.nodes[bound_leaf].relaxation;
        require(!leaf.dual.empty(), "bound leaf carries a dual certificate");
        for (double& d : leaf.dual) d *= 1.5; // infeasible dual claiming a better bound
        const auto report = verify::verify_mip_proof(model, forged);
        require(!report.accepted, "forged dual bound rejected");
        require(contains(report.message, "leaf relaxation witness"),
                "forged dual names the witness failure");
    }
    {
        auto forged = proof;
        forged.nodes[bound_leaf].relaxation.objective += 0.5; // off its certificate
        const auto report = verify::verify_mip_proof(model, forged);
        require(!report.accepted, "overstated leaf objective rejected");
        require(contains(report.message, "leaf relaxation witness"),
                "forged leaf objective names the witness failure");
    }
    {
        auto forged = proof;
        forged.objective = -5.0; // incumbent claims a better optimum than proven
        const auto report = verify::verify_mip_proof(model, forged);
        require(!report.accepted, "overstated incumbent objective rejected");
        require(contains(report.message, "incumbent"), "incumbent message");
    }

    // --- §7.1: out-of-domain and unresolved records
    {
        auto forged = proof;
        forged.nodes[0].variable = 5; // beyond the single-variable model
        const auto report = verify::verify_mip_proof(model, forged);
        require(!report.accepted && contains(report.message, "partition"),
                "out-of-domain split variable rejected");
    }
    {
        // Two columns: X binary, Y continuous (default MPS bounds). Row
        // YCAP forces a fractional LP vertex on X so the independent tree
        // splits; Y must never be selected for an integer partition.
        auto model_two = io::parse_mps_string(
            "NAME CTWO\nROWS\n N COST\n L CAP\n L YCAP\n"
            "COLUMNS\n X COST -1 CAP 1\n Y COST -1 CAP 1\n Y YCAP 1\n"
            "RHS\n R CAP 1.5 YCAP 0.4\nBOUNDS\n LI B X 0\n UI B X 2\nENDATA\n");
        auto two = verify::build_mip_proof(model_two, {1, 0.4}, -1.4, false);
        std::size_t split_index = two.nodes.size();
        for (std::size_t i = 0; i < two.nodes.size(); ++i)
            if (two.nodes[i].kind == verify::MipProofKind::split) split_index = i;
        require(split_index < two.nodes.size(), "two-column baseline splits on X");
        const auto closed = verify::verify_mip_proof(model_two, two);
        require(closed.accepted, "two-column baseline proof accepted");
        two.nodes[split_index].variable = 1; // Y is continuous
        const auto report = verify::verify_mip_proof(model_two, two);
        require(!report.accepted && contains(report.message, "partition"),
                "split on continuous variable rejected");
    }
    {
        auto forged = proof;
        forged.nodes[1].kind = verify::MipProofKind::open; // never closed
        const auto report = verify::verify_mip_proof(model, forged);
        require(!report.accepted && contains(report.message, "unresolved"),
                "open leaf rejected");
    }

    // --- §7.1: structurally invalid cut/propagation obligations
    {
        auto forged = proof;
        verify::MipObligation out_of_domain;
        out_of_domain.kind = verify::MipObligationKind::cut;
        out_of_domain.coefficients = {1.0, 2.0}; // model has a single column
        out_of_domain.rhs = 2.0;
        out_of_domain.observed_lhs = 1.5;
        forged.obligations.push_back(out_of_domain);
        const auto report = verify::verify_mip_proof(model, forged);
        require(!report.accepted && contains(report.message, "cut obligation"),
                "out-of-domain cut obligation rejected");
    }
    {
        auto forged = proof;
        verify::MipObligation nonfinite;
        nonfinite.kind = verify::MipObligationKind::cut;
        nonfinite.coefficients = {std::numeric_limits<double>::quiet_NaN()};
        nonfinite.rhs = 2.0;
        nonfinite.observed_lhs = 1.5;
        forged.obligations.push_back(nonfinite);
        const auto report = verify::verify_mip_proof(model, forged);
        require(!report.accepted && contains(report.message, "cut obligation"),
                "non-finite cut obligation rejected");
    }
    {
        auto forged = proof;
        verify::MipObligation bad_index;
        bad_index.kind = verify::MipObligationKind::propagation;
        bad_index.source_row = 9; // model has a single row
        bad_index.variable = 0;
        bad_index.source_coefficient = 1.0;
        bad_index.source_rhs = 1.0;
        bad_index.derived_bound = 1.0;
        forged.obligations.push_back(bad_index);
        const auto report = verify::verify_mip_proof(model, forged);
        require(!report.accepted && contains(report.message, "propagation obligation"),
                "out-of-domain propagation obligation rejected");
    }

    // --- §7.1: a false proof is never rescued by legitimizing cuts
    {
        auto false_proof = proof;
        false_proof.nodes[bound_leaf].relaxation.dual.clear(); // broken leaf
        verify::record_cut_obligation(false_proof, 1, {1.0}, 2.0, 1.5);
        require(!verify::verify_mip_proof(model, false_proof).accepted,
                "valid-looking cuts never rescue a false proof");
    }
    {
        verify::MipProof annotated = proof;
        verify::record_cut_obligation(annotated, 1, {1.0}, 2.0, 1.5);
        verify::record_propagation_obligation(annotated, 1, 0, 0, 1.0, 1.5, 1.5, true);
        const auto report = verify::verify_mip_proof(model, annotated);
        require(report.accepted && contains(report.message, "audit annotations"),
                "well-formed obligations stay annotations on a true proof");
    }

    // --- §7.4: production child construction matches the replay split rules
    {
        const double lows[] = {0.0, 1.4, -2.5, 2.5, 5.5};
        const double highs[] = {3.0, 5.7, 10.0, 8.25, 6.0};
        const double values[] = {0.5, 1.75, 2.4, 4.5, 5.6, 5.999};
        std::size_t checked = 0;
        for (double lo : lows)
            for (double hi : highs)
                for (double v : values) {
                    if (v < lo || v > hi) continue;
                    const auto split = milp::evaluate_split(
                        v, model::Bound::finite(lo), model::Bound::finite(hi));
                    if (!split.down_valid || !split.up_valid) continue;
                    require(split.floor_value < split.ceil_value,
                            "production partition keeps a gap");
                    verify::mip_detail::Domain parent;
                    parent.node = 0;
                    parent.lower = {model::Bound::finite(lo)};
                    parent.upper = {model::Bound::finite(hi)};
                    verify::MipProofNode record;
                    record.kind = verify::MipProofKind::split;
                    record.variable = 0;
                    record.split_value = split.floor_value;
                    auto [down, up] = verify::mip_detail::children(parent, record);
                    require(down.upper[0].is_finite() &&
                                down.upper[0].value == split.floor_value,
                            "replay down child matches production floor");
                    require(up.lower[0].is_finite() &&
                                up.lower[0].value == split.ceil_value,
                            "replay up child matches production ceil");
                    require(!verify::mip_detail::empty(down) &&
                                !verify::mip_detail::empty(up),
                            "both production children are non-empty domains");
                    ++checked;
                }
        require(checked >= 20, "partition grid exercised production vs replay");
        // §7.4: an integral branch value is a degenerate split (floor == ceil);
        // production treats it as P6 unsolved, never as a partition.
        const auto integral = milp::evaluate_split(
            5.0, model::Bound::finite(0.0), model::Bound::finite(10.0));
        require(!(integral.floor_value < integral.ceil_value),
                "integral branch value is degenerate (P6), never a split");
        std::cout << "[+] production/replay split cross-check (" << checked
                  << " grid cells) passed\n";
    }

    // --- §6.1: search proof obligations record against the model fingerprint
    {
        const std::string fingerprint =
            std::to_string(model::hash_model(model).fingerprint());
        const auto bound = verify::build_mip_proof(model, {1}, -1, false, {}, fingerprint);
        const auto report = verify::verify_mip_proof(model, bound);
        require(report.accepted &&
                    report.tier == verify::MipAssuranceTier::independent_tree,
                "bound fingerprint upgrades the accepted proof to independent_tree");
        auto stripped = bound;
        stripped.model_fingerprint.clear(); // artifact tamper: fingerprint removed
        const auto stripped_report = verify::verify_mip_proof(model, stripped);
        require(stripped_report.accepted &&
                    stripped_report.tier == verify::MipAssuranceTier::replayed_tree,
                "stripped fingerprint downgrades to replayed_tree, never rejected-or-elevated");
        require(contains(stripped_report.message, "model fingerprint unbound"),
                "stripped artifact discloses the unbound fingerprint");
        auto mismatch = bound;
        mismatch.model_fingerprint += "0";
        require(!verify::verify_mip_proof(model, mismatch).accepted,
                "mismatched fingerprint rejected before any obligation");
    }
    {
        // Engine end-to-end: the certified artifact attaches with the
        // fingerprint of the model that was actually solved.
        api::SolveOptions solve_options;
        solve_options.engine = "milp";
        const auto solved = api::solve_model(model, solve_options);
        require(solved.mip_proof != nullptr && solved.canonical_verified,
                "engine attaches the accepted proof artifact");
        require(!solved.proof_model_fingerprint.empty() &&
                    solved.mip_proof->model_fingerprint == solved.proof_model_fingerprint,
                "attached proof carries the model fingerprint");
        require(solved.proof_model_fingerprint ==
                    std::to_string(model::hash_model(model).fingerprint()),
                "recorded fingerprint matches the solved model");
    }

    std::cout << "All adversarial proof tests PASSED successfully!\n";
}
