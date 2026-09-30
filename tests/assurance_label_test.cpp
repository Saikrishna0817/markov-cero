// Contract v1 assurance labels (docs/contracts/numerical-policy.md section 5).
// derive_assurance is the single source of truth for SolveResult::assurance:
// engines never set it, and a check that did not pass never raises the label.
#include "markov_cero/api/solve.hpp"

#include "../src/api/api_internal.hpp"

#include <cstdlib>
#include <iostream>

namespace {
using namespace markov_cero;

void req(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        std::exit(1);
    }
}

api::SolveResult blank() {
    api::SolveResult r;
    r.status = lp::reference::SolveStatus::numerical_failure;
    r.certificate_type = "none";
    return r;
}

std::string label(const api::SolveResult& r) { return api::detail::derive_assurance(r); }

void label_contract() {
    auto r = blank();
    req(label(r) == "unverified", "no passed check -> unverified");

    r = blank();
    r.proof_status = "accepted";
    r.guarantee_tier = "independent_tree";
    r.canonical_verified = true;
    req(label(r) == "tree_replayed", "replayed proof tree -> tree_replayed");
    r.canonical_verified = false;
    req(label(r) == "unverified", "unreplayed proof tree -> unverified");

    r = blank();
    r.certificate_type = "local_kkt";
    r.canonical_verified = true;
    r.original_verified = true;
    req(label(r) == "local_kkt_checked", "checked KKT candidate -> local_kkt_checked");
    r.canonical_verified = false;
    req(label(r) == "unverified", "unchecked KKT candidate -> unverified");

    r = blank();
    r.status = lp::reference::SolveStatus::optimal;
    r.certificate_type = "canonical_lp_witness";
    r.verified = true;
    r.canonical_verified = true;
    req(label(r) == "optimality_witness_checked", "verified LP witness -> optimality_witness_checked");

    // A non-global status never carries a global claim, however complete the
    // witness flags are.
    r.status = lp::reference::SolveStatus::feasible;
    r.original_verified = true;
    req(label(r) == "original_primal_checked", "feasible status is not a global claim");

    // Solver-trusted MINLP outer-approximation bounds are capped at the primal
    // check even when every verification flag is set.
    r = blank();
    r.status = lp::reference::SolveStatus::optimal;
    r.certificate_type = "solver_trusted_oa_bound";
    r.verified = true;
    r.canonical_verified = true;
    r.original_verified = true;
    req(label(r) == "original_primal_checked", "solver-trusted OA capped at primal check");

    // apply_resource_stop may downgrade the status after verification: the
    // label follows the stopped status, not the pre-stop one.
    r = blank();
    r.status = lp::reference::SolveStatus::resource_limit;
    r.original_verified = true;
    req(label(r) == "original_primal_checked", "resource stop keeps only the primal check");
}

api::SolveResult tiny_lp(bool with_integer) {
    api::SolveResult r;
    model::Model m;
    m.name = "assurance_lp";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective = {-2.0, -3.0};
    m.variable_name = {"x1", "x2"};
    m.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    m.variable_upper = {model::Bound::positive_infinity(), model::Bound::positive_infinity()};
    m.variable_type = {
        with_integer ? model::VariableType::integer : model::VariableType::continuous,
        model::VariableType::continuous};
    m.row_name = {"c1", "c2"};
    m.row_lower = {model::Bound::negative_infinity(), model::Bound::negative_infinity()};
    m.row_upper = {model::Bound::finite(8.0), model::Bound::finite(10.0)};
    model::SparseMatrixBuilder mb(2, 2);
    mb.add(0, 0, 1.0);
    mb.add(0, 1, 2.0);
    mb.add(1, 0, 2.0);
    mb.add(1, 1, 1.0);
    m.matrix = mb.build();
    m.validate();
    api::SolveOptions options;
    options.engine = with_integer ? "auto" : "simplex";
    return api::solve_model(m, options);
}

void end_to_end() {
    const auto lp = tiny_lp(false);
    req(lp.status == lp::reference::SolveStatus::optimal, "LP reaches optimal");
    req(lp.assurance == "optimality_witness_checked",
        "verified LP solve labels optimality_witness_checked");

    const auto mip = tiny_lp(true);
    req(mip.status == lp::reference::SolveStatus::optimal, "MILP reaches optimal");
    req(mip.proof_status == "accepted", "MILP proof accepted");
    req(mip.assurance == "tree_replayed", "accepted proof replay labels tree_replayed");
}
} // namespace

int main() {
    label_contract();
    end_to_end();
    std::cout << "assurance label tests passed\n";
    return 0;
}
