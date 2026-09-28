---
type: codebase-test
tags: [codebase, tests, gaps]
status: verified
verified_on: 2026-09-25
evidence:
  - "CMakeLists.txt:166-224"
  - "evidence/netlib_extended.csv"
  - "docs/audit/00-ground-truth.md:184,188,193"
---

# Testing Gaps

> The suite proves correctness on small inputs; it does not test scale, statistics, or outside baselines.

## Observed Facts
- No cross-solver comparison test: `rg -i "highs|cplex|gurobi|cbc|scip"` over `evidence/`, `reports/`, `benchmarks/` returns zero hits; no test target references any external solver (all 43 targets in `CMakeLists.txt:166-224` invoke markov-cero binaries or Python runners).
- No statistical testing: runners execute each instance once, no repeats/means/stddev/CI columns (`scripts/run_netlib.py`, `run_miplib.py`, `run_gpu.py` — zero matches for repeat/mean/median).
- Largest real instance ever measured in `evidence/`: `SCORPION` 389 rows / 358 cols (`evidence/netlib_extended.csv`); largest MIPLIB: `FLUGPL` 18 rows (`evidence/miplib_results.csv`). Nothing ≥10k rows in evidence; synthetic `SCALE_10000` (5100 rows/10000 cols) appears only in `evidence/benchmarks/crossover_study.csv`.
- No Mittelmann benchmark set anywhere (`rg -i mittelmann` matches only prose docs) — corroborated at `docs/audit/00-ground-truth.md:193`.
- No interior-point/crossover test exists; PDLP is first-order (`docs/audit/00-ground-truth.md:184`). No `docs/codebase` test for basis extraction.
- No test asserts parallel speedup >1 or cut effectiveness >0% (numbers are recorded only in `evidence/benchmarks/phase4.json:39,60`).
- GPU CI absent: `.github/workflows/ci.yml` has no CUDA job and never sets `MARKOV_CERO_ENABLE_CUDA` (`CMakeLists.txt:7` default OFF).
- `json_records` only parses JSON files for syntax (`CMakeLists.txt:197`); no schema/units validation.
- Ill-conditioned / highly degenerate curated instances: no evidence file records condition numbers or perturbation stress runs (`evidence/` has no such column or file).

## Impact (Inference)
- R13 (ill-conditioning), R15 (library breadth), R16 (external comparison), R17 (robustness dossier), R20 (scale) are untested, so CI green does not imply PS compliance.
- GPU code paths ship unexercised by default CI.

## Related
- [[overview]] · [[benchmark-suites]] · [[no-external-baseline]] · [[missing-hardware-metadata-in-evidence]]
