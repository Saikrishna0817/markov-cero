"""Public Python assurance and proof-budget fields."""

import os

import markov_cero as mc


def test_milp_proof_guarantee_fields():
    m = mc.Model()
    m.integer_var(name="x", lb=0, ub=2)
    m.minimize([-1.0])
    m.add_constraint([1.0], ub=1.5, name="cap")
    accepted = m.solve(engine="milp")
    assert accepted["status"] == "Optimal"
    assert accepted["verified"] is True
    assert accepted["guarantee_tier"] == "independent_tree"
    assert accepted["proof_status"] == "accepted"
    assert accepted["proof_budget_exhausted"] is False
    assert accepted["proof_budget_kind"] == "none"
    assert accepted["proof_nodes_used"] == accepted["proof_checked_nodes"] >= 3
    assert accepted["proof_witness_values_used"] > 0
    assert accepted["proof_checked_witness_values"] > 0
    assert accepted["proof_budget_time_ms"] >= 0
    assert accepted["mip_proof_build_ms"] >= 0
    assert accepted["mip_proof_verify_ms"] >= 0
    assert accepted["proof_format_version"] >= 2
    assert accepted["proof_model_fingerprint"] == str(accepted["model_fingerprint"])
    # Contract v1 (docs/contracts/numerical-policy.md): the derived label is a
    # public result key, and an accepted replay is its strongest form.
    assert accepted["assurance"] == "tree_replayed"

    exhausted = m.solve(engine="milp", proof_max_nodes=1)
    # Blueprint §5: proof exhaustion must never be converted to Optimal.
    # The incumbent stays original-verified, but optimality is not certified.
    assert exhausted["status"] == "Feasible"
    assert exhausted["original_verified"] is True
    assert exhausted["verified"] is False
    assert exhausted["certificate_type"] == "incumbent_feasibility"
    assert exhausted["guarantee_tier"] == "unverified"
    assert exhausted["proof_status"] == "exhausted"
    assert exhausted["proof_budget_exhausted"] is True
    assert exhausted["proof_budget_kind"] == "node_limit"
    # A proof that never completed may only claim the original primal check.
    assert exhausted["assurance"] == "original_primal_checked"


def test_minlp_oa_proof_guarantee_fields():
    """MINLP-02 contract §7: OA replay fields and the exhaustion downgrade."""
    root = os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    )
    fixture = os.path.join(root, "tests", "fixtures", "minlp_case_a.mps")
    accepted = mc.solve(fixture)
    assert accepted["status"] == "Optimal"
    assert accepted["verified"] is True
    assert accepted["assurance"] == "oa_replayed"
    assert accepted["guarantee_tier"] == "independent_oa"
    assert accepted["proof_status"] == "accepted"
    assert accepted["proof_budget_exhausted"] is False
    assert accepted["certificate_type"] == "incumbent_feasibility; independent_oa_gap"
    assert accepted["oa_proof_build_ms"] >= 0
    assert accepted["oa_proof_verify_ms"] >= 0
    assert accepted["proof_model_fingerprint"] == str(accepted["model_fingerprint"])
    assert isinstance(accepted["oa_proof"], str)
    assert accepted["oa_proof"].startswith("MARKOV_OA_PROOF 1\n")

    # Contract §5.3: exhaustion keeps the original primal check but never
    # claims optimality, while the proof itself stays attached for replay.
    exhausted = mc.solve(fixture, proof_max_nodes=1)
    assert exhausted["status"] == "Feasible"
    assert exhausted["original_verified"] is True
    assert exhausted["verified"] is False
    assert exhausted["guarantee_tier"] == "unverified"
    assert exhausted["proof_status"] == "exhausted"
    assert exhausted["proof_budget_exhausted"] is True
    assert exhausted["proof_budget_kind"] == "node_limit"
    assert exhausted["assurance"] == "original_primal_checked"
    assert "optimality not certified" in exhausted["message"]
    assert isinstance(exhausted["oa_proof"], str)
    assert exhausted["oa_proof"].startswith("MARKOV_OA_PROOF 1\n")
