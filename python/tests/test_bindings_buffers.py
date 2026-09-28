"""W7 verification gate: the locked Python API surface (plan W7.1).

Covers: mc.solve on MPS files, the Model builder (LP + MILP), the NLP callback
API (D-01 Path A), and NLOBJ file models (Path B) reaching the SQP engine.
"""
import math
import os
import sys

import pytest

 # Import the installed wheel; an in-tree extension can hide packaging failures.

import markov_cero as mc  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ---------------------------------------------------------------------------
# mc.solve on files
# ---------------------------------------------------------------------------

def test_file_solve_returns_zero_copy_array(tmp_path):
    np = pytest.importorskip("numpy")
    mps = tmp_path / "buf.mps"
    mps.write_text(
        "NAME          BUF\n"
        "ROWS\n"
        " N  obj\n"
        " L  c1\n"
        "COLUMNS\n"
        "    x         obj       1.0     c1        1.0\n"
        "    y         obj       2.0     c1        1.0\n"
        "RHS\n"
        "    rhs       c1        4.0\n"
        "BOUNDS\n"
        " UP bnd       x         4.0\n"
        " UP bnd       y         4.0\n"
        "ENDATA\n"
    )
    res = mc.solve(str(mps))
    assert res["status"] == "Optimal"
    x = res["x"]
    assert isinstance(x, np.ndarray)
    assert x.base is not None
    # min x + 2y, x + y <= 4, x, y >= 0 -> origin, objective 0.
    assert x[0] == pytest.approx(0.0, abs=1e-6)
    assert x[1] == pytest.approx(0.0, abs=1e-6)


def test_nlp_numpy_buffers_and_strided_inputs():
    np = pytest.importorskip("numpy")
    nlp = mc.NlpModel()
    nlp.n_var = 2
    nlp.set_objective(
        lambda x: float(np.sum((x - np.array([1.0, -2.0])) ** 2)),
        lambda x: 2.0 * (x - np.array([1.0, -2.0])),  # ndarray gradient
    )
    nlp.set_bounds(
        np.array([-10.0, -10.0]),  # buffer bounds
        np.array([10.0, 10.0]),
    )
    x0 = np.array([0.0, 0.0])[::-1]  # strided (negative-stride) view of [0, 0]
    res = nlp.solve(x0)
    assert res["status"] == "LocalOptimal"
    assert isinstance(res["x"], np.ndarray)
    assert res["x"].base is not None
    assert res["x"][0] == pytest.approx(1.0, abs=1e-4)
    assert res["x"][1] == pytest.approx(-2.0, abs=1e-4)


def test_nlp_jacobian_2d_buffer_path():
    np = pytest.importorskip("numpy")
    nlp = mc.NlpModel()
    nlp.n_var = 2
    nlp.set_objective(
        lambda x: x[0] ** 2 + x[1] ** 2,
        lambda x: [2.0 * x[0], 2.0 * x[1]],
    )
    nlp.add_inequality(
        lambda x: np.array([1.5 - x[0] - x[1]]),   # 1-D buffer row
        lambda x: np.array([[-1.0, -1.0]]),         # 2-D float64 buffer
    )
    nlp.set_bounds([0.0, 0.0], [3.0, 3.0])
    res = nlp.solve([0.2, 0.2])
    assert res["status"] == "LocalOptimal"
    assert res["x"][0] == pytest.approx(0.75, abs=1e-4)
    assert res["x"][1] == pytest.approx(0.75, abs=1e-4)


def test_buffer_dtype_guard_rejects_non_float64():
    np = pytest.importorskip("numpy")
    m = mc.Model()
    m.continuous_var(name="x", lb=0.0, ub=1.0)
    with pytest.raises(ValueError):
        m.minimize(np.array([1.0], dtype=np.float32))
    with pytest.raises(ValueError):
        m.add_constraint(np.array([1], dtype=np.int64), ub=1.0)
