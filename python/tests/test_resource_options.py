"""Resource-budget API option tests."""

import pytest
import markov_cero as mc

def test_queued_node_limit_option():
    options = mc.SolveOptions()
    assert options.max_queued_nodes == 50000
    options.max_queued_nodes = 12
    assert options.max_queued_nodes == 12
    with pytest.raises(ValueError):
        options.max_queued_nodes = 0


def test_solve_queued_node_limit_keyword(tmp_path):
    mps = tmp_path / "queued_option_lp.mps"
    mps.write_text(
        "NAME TINY_LP\nROWS\n N COST\n L R1\nCOLUMNS\n"
        " X1 COST 1 R1 1\nRHS\n RHS1 R1 3\n"
        "BOUNDS\n UP BND1 X1 10\nENDATA\n"
    )
    result = mc.solve(str(mps), max_queued_nodes=1)
    assert result["status"] == "Optimal"
    assert result["verified"]


def test_solve_input_byte_limit_keyword(tmp_path):
    mps = tmp_path / "input_budget.mps"
    mps.write_text("NAME X\nROWS\n N O\nENDATA\n")
    result = mc.solve(str(mps), max_input_bytes=1)
    assert result["status"] == "ResourceLimit"
    options = mc.SolveOptions()
    assert options.maximum_input_bytes is None
    options.maximum_input_bytes = 1
    assert options.maximum_input_bytes == 1


def test_solve_wide_resource_options(tmp_path):
    mps = tmp_path / "resource.mps"
    mps.write_text(
        "NAME TINY_LP\nROWS\n N COST\n L R1\nCOLUMNS\n"
        " X1 COST 1 R1 1\nRHS\n RHS1 R1 3\n"
        "BOUNDS\n UP BND1 X1 10\nENDATA\n"
    )
    options = mc.SolveOptions()
    assert options.memory_limit_bytes is None
    assert options.total_time_limit_seconds is None
    options.memory_limit_bytes = 1
    limited = mc.solve(str(mps), options=options)
    assert limited["status"] == "ResourceLimit"
    assert limited["stop_reason"] == "memory_budget_exhausted"
    assert limited["diagnostic"]["failure_site"] == "memory_budget"
    options.memory_limit_bytes = None
    options.total_time_limit_seconds = 1e-12
    expired = mc.solve(str(mps), options=options)
    assert expired["status"] == "ResourceLimit"
    assert expired["stop_reason"] == "deadline_exceeded"
    expired_keyword = mc.solve(str(mps), time_limit=1e-12)
    assert expired_keyword["stop_reason"] == "deadline_exceeded"
    assert mc.solve(str(mps), memory_limit_bytes=1)["status"] == "ResourceLimit"
    with pytest.raises(ValueError):
        mc.solve(str(mps), unexpected_resource_option=1)


def test_solve_device_memory_limit_option(tmp_path):
    mps = tmp_path / "device_budget.mps"
    mps.write_text(
        "NAME TINY_LP\nROWS\n N COST\n L R1\nCOLUMNS\n"
        " X1 COST 1 R1 1\nRHS\n RHS1 R1 3\n"
        "BOUNDS\n UP BND1 X1 10\nENDATA\n"
    )
    options = mc.SolveOptions()
    assert options.device_memory_limit_bytes is None
    # The default cpu backend installs no device budget: the option is
    # accepted, nothing is charged and the solve succeeds untouched.
    options.device_memory_limit_bytes = 1
    result = mc.solve(str(mps), options=options)
    assert result["status"] == "Optimal"
    assert result["stop_reason"] == ""
    assert result["device_memory_charged_peak_bytes"] == 0
    keyword = mc.solve(str(mps), device_memory_limit_bytes=1)
    assert keyword["status"] == "Optimal"
    # Zero is rejected at the boundary, mirroring memory_limit_bytes.
    invalid = mc.solve(str(mps), device_memory_limit_bytes=0)
    assert invalid["status"] == "InvalidOptions"
    assert "device_memory_limit_bytes" in invalid["message"]
