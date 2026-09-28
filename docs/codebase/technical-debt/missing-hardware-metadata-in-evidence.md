---
type: codebase-tech-debt
tags: [codebase, technical-debt, evidence, benchmarks]
severity: high
status: verified
verified_on: 2026-09-25
evidence:
  - "evidence/environment-local.json"
  - "evidence/netlib_results.csv:1"
  - "evidence/benchmarks/phase4.json:2"
  - "evidence/benchmarks/crossover_study.csv:1"
---

# Missing Hardware Metadata in Evidence

> No evidence file records the CPU or GPU model, core count, driver, or CUDA version — benchmark numbers are not reproducible.

## Observed Facts
- `rg -i "cpu|gpu|hardware|xeon|ryzen|rtx|nvidia|processor|core|model" evidence/*.csv evidence/*.json evidence/benchmarks/*.json` → only two hits, both `"model": "blend.mps"` / `"refinery-feasible.mps"` in `evidence/benchmarks/phase4.json:5,18` (instance names, not hardware).
- `evidence/environment-local.json` records only `os`, `machine` (`x86_64`), `python` — no CPU/GPU/driver fields.
- CSV headers carry timings but no host columns: `evidence/netlib_results.csv:1`, `evidence/miplib_results.csv:1`, `evidence/benchmarks/crossover_study.csv:1`.
- GPU artifacts record kernel occupancy/reg counts but not the device name: `evidence/benchmarks/gpu_profile_summary.json` (occupancy strings only), `evidence/benchmarks/nsight_profile_analysis.md`.
- Independently flagged as PS-GAP-05 at `docs/audit/00-ground-truth.md:257`.

## Impact (Inference)
- All time-based claims (runtime_ms, 0.56× parallel speedup, GPU speedups, "29,320×") cannot be reproduced or fairly compared — including for [[Geometric Mean Runtime]]-style reporting required by external benchmark methodology.
- Blocks a credible answer to R8/R16/R20 evaluations.

## Related
- [[no-external-baseline]] · [[negative-parallel-scaling]] · [[testing-gaps]]
