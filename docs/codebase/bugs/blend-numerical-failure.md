---
type: codebase-bug
tags: [codebase, bug, benchmarks, numerics]
severity: high
status: verified
verified_on: 2026-09-25
evidence:
  - "_deployment-phase2-build/gpu_benchmark.csv:3"
  - "scripts/run_gpu.py:241-245"
  - "scripts/run_gpu.py:21"
  - "CMakeLists.txt:215-219"
---

# BLEND Simplex NumericalFailure Masked As Pass

> On Netlib BLEND the simplex engine returns `NumericalFailure`, yet the benchmark row reports `pass=True`.

## Observed Facts
- `_deployment-phase2-build/gpu_benchmark.csv:3`: `BLEND,75,84,-30.8121498457,NumericalFailure,0,0,24.701434,Optimal,...,True,True` — `simplex_status=NumericalFailure`, `simplex_objective=0`, `simplex_iterations=0`, while CPU PDLP (`Optimal`, −30.81188) and GPU PDLP (`Optimal`, −30.81204) succeed; trailing `gpu_verified=True, pass=True`.
- A second copy with a different column set also shows the failure: `_deployment-phase2-build/reports/gpu_benchmark.csv:3` (`BLEND,74,83,491,-30.8121498457,NumericalFailure,...`).
- Root cause of the masked status: `scripts/run_gpu.py:241-245` defines `inst_passed = (res_gpu["status"] == "Optimal" and res_gpu["verified"] and res_cpu["status"] == "Optimal")` — `res_simplex["status"]` is never consulted.
- Therefore the CTest target `gpu_benchmarks` (`CMakeLists.txt:215-219`, instances `afiro blend sc50a sc50b`) exits 0 even with the simplex failure; `evidence/local-verification-report.txt:232` shows `Test #40: gpu_benchmarks … Passed 0.08 sec`.
- Instance mapping: `blend` in `run_gpu.py:21` is Netlib BLEND 75×84. The CLI smoke test uses a different, tiny file: `CMakeLists.txt:203` runs `examples/blend.mps` (13 lines), while `data/netlib/blend.mps` is 359 lines — so no test exercises the failing model via simplex.
- Latest `reports/gpu_benchmark.csv` (gitignored) contains only an `AFIRO` row — the run that produced it did not complete the 4-instance list.

## Impact (Inference)
- A headline Netlib instance fails in the flagship engine while CI reports green; the `pass` column overstates reliability (R9/R15).
- Two different files named `blend.mps` make "blend passes" easy to misread as covering Netlib BLEND.

## Related
- [[mps-parser-limitations]] · [[benchmark-suites]] · [[root-only-cuts]]
