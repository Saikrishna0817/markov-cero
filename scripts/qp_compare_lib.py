"""Model loading and competitor solvers for the QP-01 benchmark (§6).

Loads the fixed-section QUADOBJ MPS files under ``data/qp/`` (OBJSENSE,
ROWS, COLUMNS, QUADOBJ, RHS, RANGES, BOUNDS) into the arrays the competitor
solvers need, and wraps OSQP and HiGHS behind one result shape. Benchmark
tooling only — this module is never linked into the solver itself.
"""
from __future__ import annotations

import subprocess
from dataclasses import dataclass, field

import numpy as np

AGREE_TOL = 1e-4  # contract §2 verifier acceptance scale (AGREE_TOL)


@dataclass
class QpModel:
    path: str
    sense: str  # "min" or "max"
    var_names: list = field(default_factory=list)
    row_names: list = field(default_factory=list)  # constraint rows only
    row_types: list = field(default_factory=list)  # L / G / E per row
    q: np.ndarray = None
    rows: dict = None  # (row, col) -> coefficient
    row_rhs: dict = field(default_factory=dict)
    row_range: dict = field(default_factory=dict)
    bounds: dict = field(default_factory=dict)  # var index -> (lo, hi)

    def row_bounds(self):
        lowers, uppers = [], []
        for name, kind in zip(self.row_names, self.row_types):
            rhs = self.row_rhs.get(name, 0.0)
            if name in self.row_range:
                rng = self.row_range[name]
                if kind == "L":
                    lo, hi = sorted((rhs, rhs - rng))
                elif kind == "G":
                    lo, hi = sorted((rhs, rhs + rng))
                else:  # E with range: rhs <= Ax <= rhs + |rng|
                    lo, hi = rhs, rhs + abs(rng)
            else:
                lo, hi = {"L": (-np.inf, rhs), "G": (rhs, np.inf),
                          "E": (rhs, rhs)}[kind]
            lowers.append(lo)
            uppers.append(hi)
        return np.asarray(lowers), np.asarray(uppers)


def parse_qp_mps(path: str) -> QpModel:
    sense = "min"
    row_types: dict[str, str] = {}
    order: list[str] = []
    obj_name = None
    columns: dict[str, dict[str, float]] = {}
    quad: list[tuple[str, str, float]] = []
    rhs: dict[str, float] = {}
    ranges: dict[str, float] = {}
    bound_records: dict[str, tuple[str, float | None]] = {}
    section = None
    with open(path) as handle:
        for line in handle:
            stripped = line.strip()
            if not stripped or stripped.startswith("*"):
                continue
            token = stripped.split()
            head = token[0]
            if head in ("OBJSENSE", "ROWS", "COLUMNS", "QUADOBJ", "RHS",
                        "RANGES", "BOUNDS", "ENDATA"):
                section = head
                if head == "ENDATA":
                    break
                if head == "OBJSENSE" and len(token) > 1:
                    sense = "max" if token[1].upper().startswith("MAX") else "min"
                continue
            if section == "OBJSENSE":
                sense = "max" if token[0].upper().startswith("MAX") else "min"
            elif section == "ROWS":
                row_types[token[1]] = token[0]
                order.append(token[1])
                if token[0] == "N" and obj_name is None:
                    obj_name = token[1]
            elif section == "COLUMNS":
                var, rest = token[0], token[1:]
                if len(rest) >= 2 and rest[0] == "'MARKER'":
                    continue
                target = columns.setdefault(var, {})
                for k in range(0, len(rest) - 1, 2):
                    target[rest[k]] = target.get(rest[k], 0.0) + float(rest[k + 1])
            elif section == "QUADOBJ":
                quad.append((token[0], token[1], float(token[2])))
            elif section == "RHS":
                rest = token[1:]
                for k in range(0, len(rest) - 1, 2):
                    rhs[rest[k]] = rhs.get(rest[k], 0.0) + float(rest[k + 1])
            elif section == "RANGES":
                rest = token[1:]
                for k in range(0, len(rest) - 1, 2):
                    ranges[rest[k]] = float(rest[k + 1])
            elif section == "BOUNDS":
                bound_records[token[2]] = (token[0],
                                           float(token[3]) if len(token) > 3 else None)

    var_names = list(columns)
    index = {name: j for j, name in enumerate(var_names)}
    row_names = [r for r in order if row_types[r] != "N"]
    row_index = {name: i for i, name in enumerate(row_names)}
    n, m = len(var_names), len(row_names)

    q = np.zeros(n)
    rows: dict[tuple[int, int], float] = {}
    for var, entries in columns.items():
        j = index[var]
        if obj_name is not None:
            q[j] = entries.get(obj_name, 0.0)
        for rname, value in entries.items():
            if rname in row_index:
                rows[(row_index[rname], j)] = value

    # QUADOBJ stores each off-diagonal once; mirror to the full symmetric form.
    p_full: dict[tuple[int, int], float] = {}
    for a, b, value in quad:
        i, j = index[a], index[b]
        p_full[(i, j)] = p_full.get((i, j), 0.0) + value
        if i != j:
            p_full[(j, i)] = p_full.get((j, i), 0.0) + value

    model = QpModel(path=path, sense=sense, var_names=var_names,
                    row_names=row_names,
                    row_types=[row_types[r] for r in row_names],
                    q=q, rows=rows, row_rhs=rhs, row_range=ranges)
    bounds = {}
    for j in range(n):
        lo, hi = 0.0, np.inf  # MPS default: 0 <= x < +inf
        for var, (kind, value) in bound_records.items():
            if var != var_names[j]:
                continue
            if kind == "UP":
                hi = value
            elif kind == "LO":
                lo = value
            elif kind == "FX":
                lo = hi = value
            elif kind == "FR":
                lo, hi = -np.inf, np.inf
            elif kind == "MI":
                lo = -np.inf
            elif kind == "PL":
                hi = np.inf
        bounds[j] = (lo, hi)
    model.bounds = bounds
    model.p_full = p_full
    return model


def arrays(model: QpModel):
    """P upper-triangular entries, q (both in minimization sense), row A, l/u."""
    n = len(model.var_names)
    sign = 1.0 if model.sense == "min" else -1.0
    upper = np.array([[i, j, v * sign] for (i, j), v in model.p_full.items()
                      if i <= j], dtype=float).reshape(-1, 3)
    q = model.q * sign
    row_l, row_u = model.row_bounds()
    var_l = np.array([model.bounds[j][0] for j in range(n)])
    var_u = np.array([model.bounds[j][1] for j in range(n)])
    return upper, q, row_l, row_u, var_l, var_u


def objective(model: QpModel, x: np.ndarray) -> float:
    value = 0.5 * sum(v * x[i] * x[j] for (i, j), v in model.p_full.items())
    value += float(model.q @ x)
    return value if model.sense == "min" else -value


def primal_check(model: QpModel, x: np.ndarray) -> tuple[bool, float]:
    worst = 0.0
    activity = np.zeros(len(model.row_names))
    scale = np.zeros(len(model.row_names))
    for (i, j), value in model.rows.items():
        activity[i] += value * x[j]
        scale[i] += abs(value * x[j])
    row_l, row_u = model.row_bounds()
    for i in range(len(activity)):
        if np.isfinite(row_l[i]):
            worst = max(worst, row_l[i] - activity[i])
        if np.isfinite(row_u[i]):
            worst = max(worst, activity[i] - row_u[i])
    for j in range(len(x)):
        lo, hi = model.bounds[j]
        worst = max(worst, lo - x[j], x[j] - hi)
    # Acceptance mirrors the contract's scaled check at 1e-4.
    ok = worst <= AGREE_TOL * (1.0 + float(np.max(np.abs(activity)) if len(activity) else 0.0)
                                     + float(np.max(np.abs(x))))
    return ok, max(worst, 0.0)


def solve_osqp(model: QpModel) -> dict:
    import osqp
    import scipy.sparse as sp

    upper, q, row_l, row_u, var_l, var_u = arrays(model)
    n = len(model.var_names)
    p = sp.csc_matrix((upper[:, 2], (upper[:, 0].astype(int), upper[:, 1].astype(int))),
                      shape=(n, n))
    m = len(model.row_names)
    a_rows, a_cols, a_vals = [], [], []
    for (i, j), value in model.rows.items():
        a_rows.append(i)
        a_cols.append(j)
        a_vals.append(value)
    for j in range(n):
        a_rows.append(m + j)
        a_cols.append(j)
        a_vals.append(1.0)
    a = sp.csc_matrix((a_vals, (a_rows, a_cols)), shape=(m + n, n))
    l = np.concatenate([row_l, var_l])
    u = np.concatenate([row_u, var_u])
    problem = osqp.OSQP()
    problem.setup(P=p, q=q, A=a, l=l, u=u, eps_abs=1e-6, eps_rel=1e-6,
                  max_iter=100000, verbose=False)
    result = problem.solve()
    x = np.asarray(result.x, dtype=float) if result.x is not None else np.full(n, np.nan)
    ok, violation = primal_check(model, x)
    return {
        "status": str(result.info.status),
        "objective": objective(model, x) if np.all(np.isfinite(x)) else None,
        "iterations": int(result.info.iter),
        "primal_ok": bool(ok and np.all(np.isfinite(x))),
        "max_violation": float(violation),
    }


def solve_highs(model: QpModel) -> dict:
    import highspy

    highs = highspy.Highs()
    highs.setOptionValue("output_flag", False)
    if highs.readModel(model.path) != highspy.HighsStatus.kOk:
        return {"status": "read_error", "objective": None, "iterations": 0,
                "primal_ok": False, "max_violation": None}
    highs.run()
    status = highs.modelStatusToString(highs.getModelStatus())
    solution = highs.getSolution()
    x = np.asarray(solution.col_value, dtype=float)
    ok, violation = primal_check(model, x)
    return {
        "status": status,
        "objective": objective(model, x) if np.all(np.isfinite(x)) else None,
        "iterations": int(highs.getInfo().simplex_iteration_count
                          + highs.getInfo().ipm_iteration_count),
        "primal_ok": bool(ok and np.all(np.isfinite(x))),
        "max_violation": float(violation),
    }


def run_markov(solver: str, path: str) -> dict:
    import json

    proc = subprocess.run([solver, path, "--engine", "qp"],
                          capture_output=True, text=True, timeout=120)
    payload = json.loads(proc.stdout.splitlines()[0])
    return {key: payload.get(key) for key in
            ("status", "objective", "verified", "certificate_type",
             "assurance", "backend_actually_used", "runtime_ms",
             "lp_iterations", "admm_rho_updates")}


def run_probe(probe: str, paths: list[str]) -> dict:
    import json

    proc = subprocess.run([probe, *paths], capture_output=True, text=True,
                          timeout=300)
    proc.check_returncode()
    return {record["instance"]: record
            for record in (json.loads(line) for line in proc.stdout.splitlines())}


def rel_diff(a: float, b: float) -> float:
    return abs(a - b) / max(1.0, abs(a), abs(b))
