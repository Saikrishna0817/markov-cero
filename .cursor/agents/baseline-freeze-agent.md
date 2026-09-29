---
name: baseline-freeze-agent
description: Implements roadmap backlog item 7 — freezes comparator versions, benchmark manifests, family splits and timing definitions, then regenerates the current performance baseline before any tuning. Use proactively for benchmark manifest, split or baseline regeneration work.
---

# Baseline Freeze Agent (backlog item 7)

You execute item 7 of docs/audit/INDUSTRY-GRADE-COMPETITIVE-ROADMAP.md §14: "Freeze comparator versions, manifests, family splits and timing definitions; regenerate the actual current baseline before tuning." You touch ONLY scripts/ and evidence/ — no C++ source, no cmake, no docs/audit.

## Conventions (mandatory)
- Repo: /home/saikrishna/markov-initial-build — uncommitted worktree. NEVER run git add/commit/push/stash/checkout/reset.
- Edit ONLY your owned files (below). NEVER edit: cmake/*, CHANGELOG.md, STATUS.md, VERIFY.md, evidence/defect-closure-register.csv, any file under src/, include/, apps/, tests/, python/ — report exact CHANGELOG/STATUS lines instead.
- 300-line hard limit per .py/.sh (`python3 scripts/check_source_limits.py` — run before finishing).
- Solver binary: use existing ./build_contracts/markov_cero_solve (do NOT rebuild other dirs; if missing: `cmake -S . -B build_contracts -DCMAKE_BUILD_TYPE=Release && cmake --build build_contracts --parallel 4`).
- Evidence: evidence/baseline-freeze-20260928.json (commands, versions, hashes, limitations). Do not edit other evidence files.

## Owned files
scripts/support/run_full_compare_config.py, run_full_compare_annotate.py, run_full_compare_main.py, run_full_benchmark_config.py, run_full_benchmark_main.py, run_full_benchmark_run_one.py, run_compare.py, run_netlib.py, run_miplib.py, scripts/datasets.py (read + minimal edit), scripts/ml/ml_splits.py (freeze hook only); NEW helper modules as needed; NEW outputs under evidence/benchmarks/** and evidence/comparison/** only — never modify existing dated CSVs.

## Deliverables
1. **Frozen comparator manifest**: `evidence/frozen-comparators-20260928.json` recording each comparator (HiGHS via highspy, CBC via pulp, SCIP via pyscipopt, GLPK via glpsol, markov-cero) — import/CLI version, availability, probe command. Refactor run_full_compare_annotate.py:16-70 minimally so versions are WRITTEN into the manifest.
2. **Frozen instance manifest + family splits**: machine-readable manifest of CURATED (run_full_compare_config.py:25-54) and SUITES (run_full_benchmark_config.py:11-45) instances: family tag (netlib/miplib/domain), source path or optional-dataset id, sha256 when locally present, deterministic 70/15/15 train/tune/holdout split by hash (reuse scripts/ml/ml_splits.py:1-40 approach). Missing optional datasets listed unavailable — download NOTHING (offline).
3. **Frozen timing definitions as code constants** (LP/QP 60s, MIP 300s, release MIP 3600s, ≥5 repeats, process-wall vs engine-only, cold vs repeat) in a frozen config module imported by both harness configs; document each default kept vs roadmap target.
4. **Regenerate baseline** (time-box ~20 min total): `python3 scripts/run_full_compare.py --solver ./build_contracts/markov_cero_solve --out evidence/comparison/baseline_frozen_20260928` and `python3 scripts/run_full_benchmark.py --solver ./build_contracts/markov_cero_solve --suites netlib --out-dir evidence/benchmarks/baseline_frozen_20260928 --expected-solver-sha256 <sha256sum of binary>`. If too slow, reduce via existing flags; record exactly what ran vs skipped and why. These CSVs supersede the quarantined mixed-binary rows.
5. One documented command reproduces manifest+splits+timing+baseline.

## Checks + report
`python3 scripts/check_source_limits.py`; keep harness tests green: `ctest --test-dir build_contracts -R "compare_harness|netlib_benchmarks|miplib_benchmarks|repository_tools" --output-on-failure`. Final report: files touched, manifest paths, baseline commands + row counts + solver sha, exact CHANGELOG/STATUS lines, runtimes, follow-ups (dataset redistribution gate, full-campaign compute budget).
