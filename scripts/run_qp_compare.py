#!/usr/bin/env python3
"""QP-01 contract §6 benchmark: markov-cero against OSQP and HiGHS.

Runs the qp_kkt_benchmark probe (convexity, KKT fill, ADMM iterations),
markov-cero-solve, OSQP and HiGHS on the tracked QPLIB subset in data/qp/,
checks objective agreement at AGREE_TOL = 1e-4 and primal acceptance, then
writes evidence/qp-kkt-bench-<date>.json. Run with the comparator venv:

  .compare-venv/bin/python scripts/run_qp_compare.py \
      --probe build/qp_kkt_benchmark --solver build/markov-cero-solve
"""
import argparse
import datetime
import glob
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from qp_compare_lib import (AGREE_TOL, parse_qp_mps, rel_diff, run_markov,
                            run_probe, solve_highs, solve_osqp)

ROOT = Path(__file__).resolve().parents[1]


def is_solved_markov(record: dict) -> bool:
    return record.get("status") == "Optimal" and record.get("verified") is True


def is_solved_osqp(record: dict) -> bool:
    return str(record.get("status", "")).startswith("solved")


def agreement(markov: dict, markov_obj, other_obj, other_solved):
    if not is_solved_markov(markov) or markov_obj is None or not other_solved \
            or other_obj is None:
        return {"agrees": None, "rel_diff": None}
    diff = rel_diff(markov_obj, other_obj)
    return {"agrees": bool(diff <= AGREE_TOL), "rel_diff": diff}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, help="qp_kkt_benchmark binary")
    parser.add_argument("--solver", required=True, help="markov-cero-solve binary")
    parser.add_argument("--output", default=None, help="evidence JSON path")
    args = parser.parse_args()

    instances = sorted(glob.glob(str(ROOT / "data" / "qp" / "*.mps")))
    if not instances:
        print("no instances under data/qp/", file=sys.stderr)
        return 1

    probe_records = run_probe(args.probe, instances)
    records = []
    for path in instances:
        name = os.path.basename(path)
        model = parse_qp_mps(path)
        markov = run_markov(args.solver, path)
        osqp_result = solve_osqp(model)
        highs_result = solve_highs(model)
        probe = probe_records.get(name, {})
        record = {
            "instance": name,
            "dimensions": {
                "variables": len(model.var_names),
                "rows": len(model.row_names),
                "sense": model.sense,
            },
            "markov": markov,
            "probe": probe,
            "osqp": osqp_result,
            "highs": highs_result,
            "agreement": {
                "osqp": agreement(markov, markov.get("objective"),
                                  osqp_result["objective"],
                                  is_solved_osqp(osqp_result)),
                "highs": agreement(markov, markov.get("objective"),
                                   highs_result["objective"],
                                   highs_result["status"] == "Optimal"),
            },
        }
        records.append(record)
        print(f"{name}: markov={markov.get('status')}/{markov.get('verified')} "
              f"fill={probe.get('kkt_L_nonzeros')} iters={probe.get('admm_iterations')} "
              f"osqp={osqp_result['status']} highs={highs_result['status']} "
              f"agree_osqp={record['agreement']['osqp']['agrees']} "
              f"agree_highs={record['agreement']['highs']['agrees']}")

    def count(key, solver_key):
        return sum(1 for r in records if r["agreement"][key][solver_key] is True)

    summary = {
        "instances": len(records),
        "markov_optimal_verified": sum(1 for r in records if is_solved_markov(r["markov"])),
        "osqp_agreements": count("osqp", "agrees"),
        "highs_agreements": count("highs", "agrees"),
        "primal_accepted": sum(1 for r in records if r["osqp"]["primal_ok"])
                            + sum(1 for r in records if r["highs"]["primal_ok"]),
        "agreement_tolerance": AGREE_TOL,
        "total_kkt_fill": sum(r["probe"].get("kkt_L_nonzeros", -1) for r in records),
        "total_admm_iterations": sum(r["probe"].get("admm_iterations", 0)
                                     for r in records),
        "symbolic_reuse_total": sum(r["probe"].get("kkt_symbolic_reuse", 0)
                                    for r in records),
    }
    today = datetime.date.today().isoformat()
    stamped = datetime.date.today().strftime("%Y%m%d")
    output = Path(args.output) if args.output else \
        ROOT / "evidence" / f"qp-kkt-bench-{stamped}.json"
    payload = {
        "contract": "docs/contracts/convex-qp.md#6-test-and-benchmark-obligations",
        "date": today,
        "solvers": {
            "markov": "local build (see provenance in git revision)",
            "osqp": __import__("osqp").__version__,
            "highspy": __import__("highspy").Highs().version(),
        },
        "settings": {"osqp_eps_abs": 1e-6, "osqp_eps_rel": 1e-6,
                     "markov_engine": "qp", "agreement_tolerance": AGREE_TOL},
        "records": records,
        "summary": summary,
    }
    output.write_text(json.dumps(payload, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    print(f"wrote {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
