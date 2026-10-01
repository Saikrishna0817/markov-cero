#!/usr/bin/env python3
"""NLP-02 contract nlp-restoration.md section 8: paired robust-convergence
record for elastic restoration.

The frozen cases — the NLP-01 section 7 corpus plus the minimized failure
case `x2_minus_one` — are each run twice through the public Python binding:
restoration enabled (on) and disabled (off = the NLP-01 path). The record
captures statuses, restoration counters, iterations, callback counts and
wall times as measured, the converged counts on vs off, and the paired wall
overhead on cases that succeed in both configurations.

No global claim, no cross-solver ranking; wall times are in-process paired
numbers only, not a speed claim.
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
KST = "LocalStationary"


def _package_version() -> str:
    try:
        from importlib.metadata import version
        return version("markov-cero")
    except Exception:
        return "unknown"


def _git_revision() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True
        ).strip()
    except Exception:
        return "unknown"


def corpus():
    """Frozen NLP-01 section 7 cases plus the NLP-02 failure case."""
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
    equality.add_equality(lambda x: [x[0] + x[1] - 2.0], lambda x: [[1.0, 1.0]])

    bound_upper = mc.NlpModel()
    bound_upper.n_var = 1
    bound_upper.set_objective(
        lambda x: (x[0] - 3.0) ** 2, lambda x: [2.0 * (x[0] - 3.0)]
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

    # NLP-02 contract section 2.1: frozen failure case x2_minus_one.
    x2 = mc.NlpModel()
    x2.n_var = 1
    x2.set_objective(lambda x: (x[0] - 1.0) ** 2, lambda x: [2.0 * (x[0] - 1.0)])
    x2.add_equality(lambda x: [x[0] * x[0] - 1.0], lambda x: [[2.0 * x[0]]])

    fast = {"max_iterations": 20000}
    return [
        ("convex_qp", convex, [0.0, 0.0], {}),
        ("rosenbrock_start_a", rosen, [-1.2, 1.0], dict(fast)),
        ("rosenbrock_start_b", rosen, [0.0, 0.0], dict(fast)),
        ("equality_quadratic", equality, [3.0, 0.0], {}),
        ("bound_active_upper", bound_upper, [0.5], {}),
        ("saddle_origin", saddle, [0.0, 0.0], {}),
        ("infeasible_pair", infeasible, [0.5], dict(fast)),
        ("x2_minus_one", x2, [0.0], {}),
    ]


def run_case(name, model, x0, kwargs, restoration: bool) -> dict:
    started = time.perf_counter()
    res = model.solve(x0, elastic_restoration=restoration, **kwargs)
    wall = time.perf_counter() - started
    return {
        "case": name,
        "restoration": "on" if restoration else "off",
        "start": list(x0),
        "status": res["status"],
        "message": res["message"],
        "iterations": res["iterations"],
        "hessian_resets": res["hessian_resets"],
        "callback_evaluations": res["callback_evaluations"],
        "restoration_steps": res["restoration_steps"],
        "restoration_failures": res["restoration_failures"],
        "wall_time_seconds": round(wall, 6),
        "objective": res["objective"],
        "x": [float(v) for v in res["x"]],
        "constraint_violation": res["constraint_violation"],
        "kkt_residual": res["kkt_residual"],
        "stationarity_residual": res.get("stationarity_residual"),
        "complementarity_residual": res.get("complementarity_residual"),
        "kkt_verified": bool(res["original_verified"]),
        "certificate_type": res["certificate_type"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        default=str(ROOT / "evidence" / f"nlp02-restoration-{date.today().isoformat()}.json"),
        help="record path (default: evidence/nlp02-restoration-<today>.json)",
    )
    args = parser.parse_args()

    cases = corpus()
    # Warm-up: the first solve in the process pays one-time initialization
    # (module import, allocator, callback plumbing); keep it out of the
    # paired wall numbers.
    warm = mc.NlpModel()
    warm.n_var = 1
    warm.set_objective(lambda x: x[0] ** 2, lambda x: [2.0 * x[0]])
    warm.solve([1.0])
    runs = []
    total_started = time.perf_counter()
    for name, model, x0, kwargs in cases:
        for restoration in (True, False):
            record = run_case(name, model, x0, kwargs, restoration)
            runs.append(record)
            print(
                f"[+] {name} restoration={record['restoration']}: "
                f"{record['status']} iters={record['iterations']} "
                f"steps={record['restoration_steps']} "
                f"fail={record['restoration_failures']} "
                f"wall={record['wall_time_seconds'] * 1000:.1f}ms"
            )
    total_wall = time.perf_counter() - total_started

    def summary(flag: str) -> dict:
        subset = [r for r in runs if r["restoration"] == flag]
        by_status = {}
        for r in subset:
            by_status[r["status"]] = by_status.get(r["status"], 0) + 1
        return {
            "runs": len(subset),
            "by_status": by_status,
            "converged": sum(1 for r in subset if r["status"] == KST),
        }

    on, off = summary("on"), summary("off")
    paired_overhead = []
    for name, _, _, _ in cases:
        row = {r["restoration"]: r for r in runs if r["case"] == name}
        if row["on"]["status"] == KST and row["off"]["status"] == KST:
            paired_overhead.append({
                "case": name,
                "wall_on_seconds": row["on"]["wall_time_seconds"],
                "wall_off_seconds": row["off"]["wall_time_seconds"],
                "ratio_on_over_off": round(
                    row["on"]["wall_time_seconds"] / max(row["off"]["wall_time_seconds"], 1e-12), 4
                ),
            })

    payload = {
        "record": "nlp02-restoration-paired",
        "date": date.today().isoformat(),
        "contract": "docs/contracts/nlp-restoration.md section 8",
        "entry_point": "python binding: markov_cero.NlpModel.solve (SQP)",
        "package_version": _package_version(),
        "git_revision": _git_revision(),
        "python": sys.version.split()[0],
        "platform": platform.platform(),
        "design": (
            "Each frozen case run twice through the same build: "
            "elastic_restoration on (NLP-02) and off (NLP-01 path)."
        ),
        "corpus_size": len(cases),
        "runs": runs,
        "summary": {
            "restoration_on": on,
            "restoration_off": off,
            "converged_delta_on_minus_off": on["converged"] - off["converged"],
            "paired_wall_overhead_shared_success": paired_overhead,
            "total_wall_time_seconds": round(total_wall, 6),
        },
        "claims": (
            "Paired robust-convergence counts and wall times as measured on "
            "the frozen eight-case corpus through the public binding, on vs "
            "off, same build, same host."
        ),
        "non_claims": [
            "no speed claim beyond the paired in-process wall numbers",
            "no global objective proof and no local-minimum claim for any case",
            "no cross-solver ranking",
            "the infeasible pair remains an inconclusive honest failure, "
            "never a proof of infeasibility",
        ],
    }

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"[+] wrote {out_path} ({len(runs)} runs)")
    print(f"[+] converged on={on['converged']} off={off['converged']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
