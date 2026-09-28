---
type: codebase-architecture
tags: [codebase, architecture, target-state]
status: complete
date: 2026-09-25
---

# Target architecture (current → target)

> Short form of [[14-target-architecture]] — Phase 10. Component mapping and non-goals live there;
> the as-built baseline is [[current]]. New items are the RW numbers from [[12-keep-remove-rebuild]].

## Summary

Same six layers as today, with the evaluation layer added for the first time and four *NEW*
engine-side items: sparse IPM + crossover (R4), an in-tree cut loop with a pool (RW-1), a
load-balanced scheduler (RW-2), and iterative refinement + condition estimation (RW-8).
Orchestration moves out of the CLI into `src/api/` so R14's "API" becomes real (RW-4), and
canonicalization collapses to one sparse-first path (RW-5). Verifiers, CSC model, simplex and
ADMM cores are unchanged — they pass audit.

**Pipeline (one line):**
`MPS/LP → model (immutable CSC, single sparse-first canonicalization) → reduction (deeper presolve + Ruiz, LIFO postsolve) → engine layer via solve() API [revised | dual+DSE | PDLP CPU/GPU | IPM+crossover NEW | B&C with in-tree cuts NEW, stealing NEW | ADMM+LDLᵀ; numerics refinement NEW] → verification (unchanged, promoted) → evaluation (NEW: run_compare vs HiGHS, 4 suites, dossier, GM + Dolan–Moré, hardware manifest) → JSON + certificate → CLI/C++ API → reports`

## Diff table (from [[14-target-architecture]] "Explicit distinctions")

| Dimension | Current | Target |
|---|---|---|
| Canonicalization | 2 paths + dense gate 2048×8192 | 1 sparse-first path, dense only as internal fast path (RW-5) |
| IPM / crossover | absent (AP-1) | minimal sparse IPM + crossover, or documented re-scope (R4, step 6) |
| Cuts | root-only, 0.0 % node reduction (AP-3) | in-tree loop + cut pool, measured >20 % (RW-1) |
| Parallel | shared heap, no stealing, 0.56× (AP-4) | load-balanced scheduler, measured 1/2/4/8 curve (RW-2) |
| API | orchestration in `main` (AP-6) | `src/api/solve_*` + thin CLIs + `install()` (RW-4) |
| Evaluation | single-solver CSVs, no baseline (AP-2) | harness vs HiGHS, profiles, robustness dossier, hardware manifest (RW-3, greenfield) |
| Numerics | centralized tolerances only (AP-8) | + iterative refinement + condition estimation (RW-8) |
| GPU | claimed, losing, status bug (AP-5) | fixed measurement, honest crossover claim (RW-9) |
| Docs | drift (AP-11) | claims↔evidence table, R1–R20 coverage in STATUS (RW-10) |

## Non-goals

No GUI / web service / database / cloud (R14) · no ML branching this cycle (P3, [[ED-009-defer-ml-branching]]) ·
no NLP/MINLP engines now — extension points only (R3 judged structurally) · no rewrite of the
simplex, ADMM or verifier cores.

## Related

[[current]] · [[14-target-architecture]] · [[13-restart-point]] · [[15-roadmap]] ·
[[Architecture MOC]] · [[Codebase MOC]] · [[17-sih-demo-strategy]] (slide 2 uses this flow)
