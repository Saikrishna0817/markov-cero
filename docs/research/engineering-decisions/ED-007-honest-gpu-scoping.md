---
type: engineering-decision
id: ED-007
status: accepted
date: 2026-09-25
tags: [engineering-decision, gpu, r8, p0, claims]
---

# ED-007 — Honest GPU Scoping: Fix the Measurement, Then Narrow the Claim

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> **Revalidated 2026-09-28:** the historical 13-row CPU-fallback study below is superseded
> for current performance claims. A CUDA-enabled solver was run on the physical RTX 2050;
> four GPU PDLP solutions verified, and GPU was 2.60–8.64× slower end-to-end than CPU PDLP
> on those four cases. See `evidence/benchmarks/crossover_study_gpu_rtx2050.csv` and
> `evidence/hardware.md`. This confirms no measured benefit on the tested device and sizes.

> GPU claims are scoped to what per-scale, status-checked, hardware-recorded measurements support — and nothing else, even if that means GPU becomes an appendix.

## Historical context (captured 2026-09-25; superseded by the revalidation above)

- `evidence/benchmarks/crossover_study.csv`: end-to-end speedup < 1 in **13/13** rows, kernel speedup < 1 in 12/13 — GPU loses to CPU PDLP at every measured scale (`00-ground-truth` C.3).
- `scripts/run_gpu.py:241-245` never checks `res_simplex["status"]`, so `BLEND → NumericalFailure` still passes the `gpu_benchmarks` CTest (`evidence/local-verification-report.txt:232`).
- No CPU/GPU model recorded anywhere (PS-GAP-05); the "up to 29320×" headline is not corroborated by any committed CSV (12 §8.2 REMOVE).
- The PS makes benefit a precondition — "GPU acceleration considered where it provides measurable benefits" — so R8 is UNPROVEN, not merely unmeasured.

## Research Evidence

- [[Lu-2025-cuPDLP-GPU-Implementation]] — the cuPDLP line: GPU first-order wins are large-scale and instance-dependent by the authors' own reporting.
- [[Unknown-2025-Overview-GPU-Based-First]] — survey of GPU first-order LP: where bandwidth-bound iteration beats pivot-based methods.
- [[GPU Benefit Unproven]] — the named research-gap note for this exact repository state.
- [[First-Order Accuracy Ceiling]] — why status-checked tolerances, not kernel timings, decide the comparison.
- [[GPU CSR SpMV]] — the kernel whose timing must never be reported as a solver speedup.

## Decision

- Step 1: status checking and hardware capture have been implemented in `scripts/run_gpu.py`; static CUDA fatbin detection was added so a statically linked runtime is not mistaken for the CPU fallback.
- Step 2: one fresh, verified RTX 2050 CPU/GPU PDLP run was completed at each of four scales. Repeated trials remain open. The canonical records are `evidence/benchmarks/crossover_study.csv` and `evidence/benchmarks/crossover_study_gpu_rtx2050.csv`.
- Step 3: claims now state that GPU lost 4/4 end-to-end cases (2.60–8.64× slower); the old 29,320× comparison and 13/13 hardware inference are not current results.
- The GPU code itself is KEPT (12 §8.1) — this decision scopes *claims*, not the engine.

## Consequences (positive / negative / neutral)

- **positive:** R8's "measurable benefit" precondition is finally honored; restores trust in the evidence folder; deterministic two-stage reductions remain a genuine strength to show.
- **negative:** the headline GPU feature may shrink to an appendix before the demo; re-runs cost a CI/dev-machine slot.
- **neutral:** CUDA build flag, equivalence tests and JSON timing fields are unchanged.

## Alternatives Rejected

- *Keep the 29320× headline* — contradicted by our own CSV (12 §8.2).
- *Report kernel-only speedups* — kernel time is not solve time; forbidden by cross-paper §6.
- *Delete the GPU engine, or ship with the status bug* — the engine is a KEEP with deterministic value; an engine that hides NumericalFailure is worse than no engine.

## Linked Requirements

- R8, R20, R18 → sih26119_problem_statement

## Related

- 09-research-code-alignment (R8, §6.3, experiment E4) · 12-keep-remove-rebuild (RW-9) · 13-restart-point (step 2) · 21-traceability §21.1 R8, §21.2
- GPU-PDHG-Engine · [[ED-001-comparison-harness-before-new-algorithms]]
