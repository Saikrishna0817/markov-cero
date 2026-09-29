#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
solver="${MARKOV_CERO_SOLVE_BIN:-$repo_dir/build_item5/markov-cero-solve}"
scratch="$(mktemp -d)"
child=""
cleanup() {
    if [[ -n "$child" ]] && kill -0 "$child" 2>/dev/null; then
        kill -KILL "$child" 2>/dev/null || true
        wait "$child" 2>/dev/null || true
    fi
    rm -rf "$scratch"
}
trap cleanup EXIT

if [[ ! -x "$solver" ]]; then
    echo "solver binary not found at $solver (set MARKOV_CERO_SOLVE_BIN)" >&2
    exit 1
fi

python3 - "$scratch/hard.mps" <<'PY'
import sys
with open(sys.argv[1], "w", encoding="ascii") as out:
    out.write("NAME KILL_DEMO\nROWS\n N COST\n E HALF\nCOLUMNS\n")
    out.write(" MARK0000 'MARKER' 'INTORG'\n")
    for index in range(32):
        out.write(f" X{index} COST {index + 1} HALF 1\n")
    out.write(" MARK0001 'MARKER' 'INTEND'\nRHS\n RHS1 HALF 15.5\nBOUNDS\n")
    for index in range(32):
        out.write(f" BV BND1 X{index}\n")
    out.write("ENDATA\n")
PY

"$solver" "$scratch/hard.mps" --engine parallel --threads 4 \
    --no-cuts --no-heuristics --time-limit 10 --output "$scratch/killed.json" \
    >"$scratch/killed.stdout" 2>"$scratch/killed.stderr" &
child=$!
sleep 0.05
if ! kill -0 "$child" 2>/dev/null; then
    echo "solve ended before SIGKILL could be delivered" >&2
    exit 1
fi
kill -KILL "$child"
set +e
wait "$child" 2>/dev/null
status=$?
set -e
child=""
if [[ "$status" -ne 137 || -s "$scratch/killed.json" ]]; then
    echo "killed worker emitted a result or did not die from SIGKILL" >&2
    exit 1
fi

set +e
"$solver" "$scratch/hard.mps" --engine parallel --threads 4 \
    --no-cuts --no-heuristics --time-limit 1 --output "$scratch/rerun.json" \
    >"$scratch/rerun.stdout" 2>"$scratch/rerun.stderr"
rerun_status=$?
set -e
python3 - "$scratch/rerun.json" "$rerun_status" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as source:
    result = json.load(source)
if result["status"] not in {"ResourceLimit", "Infeasible", "Optimal"}:
    raise SystemExit("rerun did not complete with a valid solve status")
if result["status"] == "Optimal" and not result["verified"]:
    raise SystemExit("rerun claimed an unverified optimum")
print(f"SIGKILL observed; rerun status={result['status']} exit={sys.argv[2]}")
PY
