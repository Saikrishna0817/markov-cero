#!/usr/bin/env python3
"""NLP-01 contract nlp-local-sqp.md section 7: run the local NLP corpus
through the public Python entry point as a record separate from the
linear MIP / MIQP / QP summaries.

Per start it records the status as measured, the feasibility and stationarity
residuals, iterations, callback evaluations and wall time. The record is
explicitly local: no global objective proof, no cross-solver ranking and no
speed claim.
"""
import argparse
import json
import platform
import subprocess
import sys
import time
from datetime import date
from pathlib import Path

import markov_cero as mc  # installed wheel

ROOT = Path(__file__).resolve().parents[1]


def _package_version() -> str:
    try:
        from importlib.metadata import version
        return version("markov-cero")
    except Exception:
        return "unknown"


def _git_revision() -> str:
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True
        )
        return out.strip()
    except Exception:
        return "unknown"


def corpus():
    """Yield (case_name, model, x0, solve_kwargs) for the section 6 cases."""
    rosen = mc.NlpModel()
    rosen.n_var = 2
    rosen.set_objective(
        lambda x: 100.0 * (x[1] - x[0] ** 2) ** 2 + (1.0 - x[0]) ** 2,
        lambda x: [
            -400.0 * x[0] * (x[1] - x[0] ** 2) - 2.0 * (1.0 - x[0]),
            200.0 * (x[1] - x[0] ** 2),
        ],
    )

    convex = mc.NlpModel()
    convex.n_var = 2
    convex.set_objective(
        lambda x: (x[0] - 1.0) ** 2 + (x[1] - 2.0) ** 2,
        lambda x: [2.0 * (x[0] - 1.0), 2.0 * (x[1] - 2.0)],
    )
    convex.set_bounds([0.0, 0.0], [5.0, 5.0])

    equality = mc.NlpModel()
    equality.n_var = 2
    equality.set_objective(
        lambda x: x[0] ** 2 + x[1] ** 2,
        lambda x: [2.0 * x[0], 2.0 * x[1]],
    )
    equality.add_equality(
        lambda x: [x[0] + x[1] - 2.0],
        lambda x: [[1.0, 1.0]],
    )

    bound_upper = mc.NlpModel()
    bound_upper.n_var = 1
    bound_upper.set_objective(
        lambda x: (x[0] - 3.0) ** 2,
        lambda x: [2.0 * (x[0] - 3.0)],
    )
    bound_upper.set_bounds([0.0], [1.0])

    saddle = mc.NlpModel()
    saddle.n_var = 2
    saddle.set_objective(
        lambda x: x[0] ** 2 - x[1] ** 2,
        lambda x: [2.0 * x[0], -2.0 * x[1]],
    )

    infeasible = mc.NlpModel()
    infeasible.n_var = 1
    infeasible.set_objective(lambda x: x[0] ** 2, lambda x: [2.0 * x[0]])
    infeasible.add_inequality(lambda x: [x[0]], lambda x: [[1.0]])
    infeasible.add_inequality(lambda x: [1.0 - x[0]], lambda x: [[-1.0]])

    fast = {"max_iterations": 20000}
    return [
        ("convex_qp", convex, [0.0, 0.0], {}),
        ("rosenbrock_start_a", rosen, [-1.2, 1.0], dict(fast)),
        ("rosenbrock_start_b", rosen, [0.0, 0.0], dict(fast)),
        ("equality_quadratic", equality, [3.0, 0.0], {}),
        ("bound_active_upper", bound_upper, [0.5], {}),
        ("saddle_origin", saddle, [0.0, 0.0], {}),
        ("infeasible_pair", infeasible, [0.5], dict(fast)),
    ]


def run_case(name, model, x0, kwargs) -> dict:
    started = time.perf_counter()
    res = model.solve(x0, **kwargs)
    wall = time.perf_counter() - started
    record = {
        "case": name,
        "start": list(x0),
        "status": res["status"],
        "message": res["message"],
        "iterations": res["iterations"],
        "hessian_resets": res["hessian_resets"],
        "callback_evaluations": res["callback_evaluations"],
        "wall_time_seconds": round(wall, 6),
        "objective": res["objective"],
        "x": [float(v) for v in res["x"]],
        "constraint_violation": res["constraint_violation"],
        "kkt_residual": res["kkt_residual"],
        "stationarity_residual": res.get("stationarity_residual"),
        "complementarity_residual": res.get("complementarity_residual"),
        "x0_projection_norm": res["x0_projection_norm"],
        "kkt_verified": bool(res["original_verified"]),
        "certificate_type": res["certificate_type"],
    }
    if res.get("best_feasible_x") is not None:
        record["best_feasible_objective"] = res.get("best_feasible_objective")
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        default=str(ROOT / "evidence" / f"nlp01-local-{date.today().isoformat()}.json"),
        help="record path (default: evidence/nlp01-local-<today>.json)",
    )
    args = parser.parse_args()

    runs = []
    total_started = time.perf_counter()
    for name, model, x0, kwargs in corpus():
        record = run_case(name, model, x0, kwargs)
        runs.append(record)
        print(
            f"[+] {name}: {record['status']} iters={record['iterations']} "
            f"cb={record['callback_evaluations']} "
            f"viol={record['constraint_violation']:.3e} "
            f"stat={record['stationarity_residual']:.3e} "
            f"wall={record['wall_time_seconds']*1000:.1f}ms"
        )
    total_wall = time.perf_counter() - total_started

    by_status = {}
    for record in runs:
        by_status[record["status"]] = by_status.get(record["status"], 0) + 1

    payload = {
        "record": "nlp01-local-nlp-corpus",
        "date": date.today().isoformat(),
        "contract": "docs/contracts/nlp-local-sqp.md section 7",
        "entry_point": "python binding: markov_cero.NlpModel.solve (SQP)",
        "package_version": _package_version(),
        "git_revision": _git_revision(),
        "python": sys.version.split()[0],
        "platform": platform.platform(),
        "corpus_size": len(runs),
        "runs": runs,
        "summary": {
            "by_status": by_status,
            "kkt_verified_runs": sum(1 for r in runs if r["kkt_verified"]),
            "total_wall_time_seconds": round(total_wall, 6),
        },
        "claims": (
            "Statuses, feasibility/stationarity residuals, iteration and "
            "callback counts, and wall times as measured on this seven-case "
            "local corpus through the public binding."
        ),
        "non_claims": [
            "no global objective proof for any case",
            "no local-minimum (second-order) claim for any first-order point",
            "no cross-solver ranking and no speed claim",
            "the infeasible pair is an inconclusive honest failure, not a "
            "proof of infeasibility",
        ],
    }

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"[+] wrote {out_path} ({len(runs)} runs)")
    print(f"[+] summary: {by_status}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
