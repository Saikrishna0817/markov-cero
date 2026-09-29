"""Public Python assurance and proof-budget fields."""

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

    exhausted = m.solve(engine="milp", proof_max_nodes=1)
    assert exhausted["status"] == "Optimal"
    assert exhausted["original_verified"] is True
    assert exhausted["verified"] is False
    assert exhausted["certificate_type"] == "incumbent_feasibility"
    assert exhausted["guarantee_tier"] == "unverified"
    assert exhausted["proof_status"] == "exhausted"
    assert exhausted["proof_budget_exhausted"] is True
    assert exhausted["proof_budget_kind"] == "node_limit"
