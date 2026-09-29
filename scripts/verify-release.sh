#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$ROOT"
# Overridable so a redirected run can be recorded without rewriting the
# default evidence files (both remain the default when the variables are unset).
REPORT="${MARKOV_CERO_VERIFY_REPORT:-$ROOT/evidence/local-verification-report.txt}"
ENVJSON="${MARKOV_CERO_VERIFY_ENVJSON:-$ROOT/evidence/environment-local.json}"
mkdir -p "$(dirname "$REPORT")" "$(dirname "$ENVJSON")"
: > "$REPORT"
log() { printf '%s\n' "$*" | tee -a "$REPORT"; }
run() { log "+ $*"; "$@" >>"$REPORT" 2>&1; }
log "markov-cero cumulative M5 local verification"
log "UTC: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
python3 - "$ENVJSON" <<'PY'
import json, platform, sys
from pathlib import Path
payload = {"os": platform.platform(), "machine": platform.machine(), "python": platform.python_version()}
Path(sys.argv[1]).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY
run python3 scripts/check-sovereignty.py "$ROOT"
for command in cmake ctest python3; do
  if ! command -v "$command" >/dev/null 2>&1; then
    log "FAIL: required tool $command is unavailable"
    exit 1
  fi
done
blocked=0
failed=0
# Each compiler is attempted even when the other fails, so the report records
# both configurations instead of stopping at the first one.
for pair in "gcc:g++" "clang:clang++"; do
  name=${pair%%:*}; compiler=${pair##*:}
  if ! command -v "$compiler" >/dev/null 2>&1; then
    log "BLOCKED: $compiler not available"
    blocked=1
    continue
  fi
  build_dir=$(mktemp -d "${TMPDIR:-/tmp}/markov-verify-$name.XXXXXX")
  log "Build artifacts retained at $build_dir"
  if ! run cmake -S "$ROOT" -B "$build_dir" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$compiler"; then
    log "FAIL: $name configuration step"
    failed=1
    continue
  fi
  if ! run cmake --build "$build_dir" --parallel 2; then
    log "FAIL: $name build step"
    failed=1
    continue
  fi
  if ! run ctest --test-dir "$build_dir" --output-on-failure; then
    log "FAIL: $name ctest step"
    failed=1
    continue
  fi
  log "PASS: $name configuration passed all registered tests"
done
if [[ $blocked -ne 0 ]]; then
  log "INCOMPLETE: a required compiler configuration was unavailable"
  exit 1
fi
if [[ $failed -ne 0 ]]; then
  log "FAIL: at least one compiler configuration did not pass"
  exit 1
fi
log "PASS: both compiler configurations passed all registered tests"
