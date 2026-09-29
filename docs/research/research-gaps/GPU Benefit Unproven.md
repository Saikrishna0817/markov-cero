---
type: research-gap
tags: [research-gaps, gpu]
status: stable
verified_on: 2026-09-28
---

# GPU Benefit Unproven

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> RTX 2050 hardware validation completed on 2026-09-27: GPU PDLP verified on four cases, but was 2.60–8.64× slower end-to-end than CPU PDLP in all four. No GPU benefit is demonstrated on the measured scales.

## Definition
R8 is conditional: GPU acceleration is justified "where it provides measurable benefits". The physical RTX 2050 comparison now has four status-checked, independently verified GPU PDLP solves; GPU lost end-to-end on all four tested scales. The cases span 14,400–640,000 parsed nonzeros, with one run per backend per case; they do not establish behavior above that size range or statistical repeatability. The old 13-row claim and 29,320× figure are historical and do not describe this hardware validation. The remaining gap is to demonstrate a reproducible crossover on a larger workload or present the GPU path only as an experimental capability without benefit claims.

## Why It Matters Here
- R8's wording makes benefit a *precondition*, not a wish; the audit lists R8 as "UNPROVEN" in the divergence table (docs/audit/00-ground-truth.md).
- Observed state: `evidence/benchmarks/crossover_study_gpu_rtx2050.csv` has four verified hardware rows and full timing breakdowns; canonical plot data is in `evidence/benchmarks/crossover_study.csv`.
- Observed state: H2D transfer alone was 87–111 ms and the GPU used more iterations than CPU PDLP on all four cases. This is consistent with a workload/iteration-count penalty plus transfer cost, but does not isolate a single cause; profiling is still needed.

## Key Facts / Rules
- Evidence standard: per-scale CPU vs GPU end-to-end times, same tolerance, same instance, CPU/GPU model + driver recorded. The current RTX 2050 run meets that basic provenance bar; repeated trials and broader hardware coverage remain open.
- Separate kernel time from end-to-end time — only the latter is a user-visible benefit.
- Size threshold matters: GPU first-order methods lose on tiny instances (AFIRO/SC50A) and should be expected to; state the crossover point.
- Correctness gate: a run that failed status checks must not be counted as a pass.

## Related
- [[GPU CSR SpMV]]
- [[Primal-Dual Hybrid Gradient]]
- [[Parallel Speedup]]
- [[Numerical Error]]
- [[Lu-2025-cuPDLP-GPU-Implementation]]

## Referenced By

- 21-traceability
- Research MOC
- [[Primal-Dual Hybrid Gradient|research/algorithms/Primal-Dual Hybrid Gradient]]
- [[ED-007-honest-gpu-scoping|research/engineering-decisions/ED-007-honest-gpu-scoping]]
- cross-paper-synthesis
- research-dependency-map
- [[GPU CSR SpMV|research/techniques/GPU CSR SpMV]]
