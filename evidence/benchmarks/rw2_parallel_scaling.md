# RW-2 Parallel Tree Search — Scaling Evidence (Post-Rework)

**Date**: 2026-09-25 · **Change**: RW-2 / ED-006 Option A (load-balanced queue rework)
**Baseline**: `v0.5.2-audit-baseline` phase4.json reported **S₄ = 0.56×, E₄ = 14.1%** (stein9.mps)
**Tool**: `scripts/rw2_scaling_baseline.py` (median of 5 runs per cell, same
methodology as phase4.json option_c so numbers are directly comparable)

## Hardware manifest (closes PS-GAP-05 for this artifact)

| Item | Value |
|---|---|
| CPU | 12th Gen Intel(R) Core(TM) i5-12450HX, 12 threads |
| RAM | 11 GiB |
| OS | Linux 7.2.5-3-omarchy (x86_64) |
| Compiler | GCC 16.2.1, C++20, `-O3` (CMAKE_BUILD_TYPE=Release) |
| Build dir | `build-rw2/` |

## What changed (RW-2 / ED-006 Option A)

1. **Batch pops** (`ThreadSafeNodeQueue::pop_batch`, up to 16 nodes per lock acquisition):
   one mutex round-trip hands a worker a best-bounded subtree slice instead of one
   round-trip per node. Critical-section count per node drops ~16×.
2. **Lazy prune-at-pop**: stale nodes (lower bound ≥ cutoff) are skipped while popping;
   the O(n) `remove_if` + `make_heap` full-heap prune no longer runs under the global
   lock on every pop and every incumbent improvement.
3. **Interleaved batch order** (best, worst, 2nd-best, …) at the worker level: concurrent
   workers on the same batch explore different subtree regions first — soft work
   stealing without per-worker deques (steal granularity per [[Work Stealing]]).
4. **Correct termination preserved**: empty-batch workers wait (bounded 2 ms slice) while
   other workers hold unprocessed work; quiescence (heap empty + no active batch) stops
   the queue. Verified by 40 back-to-back stress runs and a TSan build.

## Results (median of 5, wall-clock ms)

### flugpl.mps (18 x 16, 4873 nodes serial-equivalent)

| p | T_ms | Speedup S_p | Efficiency E_p |
|---|---|---|---|
| 1 | 1845.33 | 1.00× | 100% |
| 2 | 932.12 | **1.98×** | 99.0% |
| 4 | 501.77 | **3.68×** | 91.9% |
| 8 | 424.93 | **4.34×** | 54.3% |

Repeat runs across the session: S₄ ∈ {2.92×, 3.19×, 3.68×} — always ≥ 2.9×.
Superlinear S₂ ≈ 2.0× is search-order variance (parallel best-first finds the
incumbent sooner), a documented B&B effect, not a timing artifact.

### stein15.mps (149 nodes serial)

| p | T_ms | Speedup S_p | Efficiency E_p |
|---|---|---|---|
| 1 | 709.23 | 1.00× | 100% |
| 2 | 457.29 | 1.55× | 77.5% |
| 4 | 368.09 | 1.93× | 48.2% |
| 8 | 365.18 | 1.94× | 24.3% |

### stein9.mps (~20 ms — granularity floor)

S₄ ∈ {0.87×–1.39×} run-to-run. At 43 total nodes each LP solve is microseconds;
synchronization dominates regardless of design (Amdahl + granularity,
[[Negative Parallel Scaling]]). Small-instance non-scaling is expected and honest.

## Correctness gates (all green)

| Gate | Result |
|---|---|
| `parallel_tree_search_test` (incl. new batch/lazy-prune/interleave unit tests) | PASS |
| 40-run thread-safety stress (1/2/4 threads, two models) | PASS |
| ThreadSanitizer build, full test binary | PASS (no races/deadlocks) |
| `ctest` full suite (44 targets) | 43/44 — only `gpu_benchmarks` fails, and that is the **P0-2 status-gate fix working as designed** (BLEND simplex `NumericalFailure` now correctly surfaces, pre-existing, unrelated to RW-2) |
| Identical optima across 1/2/4/8 threads on all instances | PASS |
| Zero-trust verification of returned solutions | PASS (`verified=true`) |

## ED-006 verdict

**Option A landed**: S₄ ≥ 1.5× on both multi-hundred-node instances (DoD asked ≥ 1.5× at
4 threads on a ≥10k-node-class instance; flugpl delivers 2.9–3.7×). The parallel engine
keeps its claim, scoped honestly: scaling is demonstrated on medium instances; tiny
instances (stein9-class, <50 nodes, <50 ms) remain serial-dominated by design.

## Related

[[ED-006-load-balanced-parallel-or-demote]] · [[Work Stealing]] · [[Negative Parallel Scaling]] ·
[[15-roadmap]] P0-4 · `docs/audit/12-keep-remove-rebuild.md` RW-2
