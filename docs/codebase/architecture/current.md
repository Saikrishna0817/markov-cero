---
type: codebase-architecture
tags: [codebase, architecture, current-state]
status: complete
date: 2026-09-25
---

# Current architecture (as built)

> Historical snapshot dated 2026-09-25. Its former clean-room provenance claim is superseded:
> peer solver source exposure is recorded in `docs/audit/19-competitive-landscape.md`, and
> independent source-trace review remains open. See `PROVENANCE.md`.

> Short form of [[07-current-architecture]] — Phase 5 reconstruction, every fact verified against
> source. Component detail: [[Codebase MOC]]. Target form: [[target]].

## Summary

Static library `markov_cero_core` + three apps (`markov-cero-solve`, `-info`, `-mps-inspect`);
CSC model, reversible LIFO presolve/postsolve, five engine families behind `--engine`,
independent dual-gated verifiers, JSON output with a status certificate. Dependency isolation,
verifiers and multi-engine dispatch are implemented; provenance status remains under independent
review. What is thin is *coverage* (IPM, cut depth, presolve depth)
and the *evaluation layer* — and evaluation is what SIH grades first.

**Pipeline (one line):**
`MPS → canonicalize (dense gate 2048×8192 | sparse) → presolve (3 rules) + Ruiz → dispatch in apps/markov_cero_solve.cpp (499-line monolith) → primal | dual(Harris) | pdlp(CPU/GPU) | milp(root-only cuts, no stealing) | qp | miqp → postsolve → verifiers → JSON`

## Problems (AP-1…AP-12, from [[07-current-architecture]] §C.4)

| # | Problem |
|---|---|
| AP-1 | No interior-point engine and no crossover — PS R4 requires both; 0 code hits (missing component) |
| AP-2 | Evaluation subsystem missing entirely — no external-solver harness, no Mittelmann set (R16 binary gap) |
| AP-3 | Cuts are root-only → cut-node reduction 0.0 %; branch-and-cut degenerates into bounded best-bound B&B |
| AP-4 | Parallel search has no work stealing / load balancing → 4 threads = 0.56×, 14 % efficiency |
| AP-5 | GPU loses 13/13 measured end-to-end yet is a headline claim; `run_gpu.py:241-245` hides a NumericalFailure |
| AP-6 | `apps/markov_cero_solve.cpp` (~500 lines) owns dispatch, verification, timing and output — no real library API (R14) |
| AP-7 | Dense-first canonicalization gate splits behaviour into two paths behind one arbitrary threshold (R12 risk) |
| AP-8 | No numerical robustness tooling: no iterative refinement, no condition estimation (R17 unsupported) |
| AP-9 | Presolve is 3 rule classes where the research baseline implies dozens |
| AP-10 | GPU path not covered by CI; determinism claims tested locally only |
| AP-11 | `docs/history.md` stops at Phase 4 while CHANGELOG/README claim Phase 6 — structural doc drift |
| AP-12 | Dead/duplicate artifacts: unwired fuzz target, md5-duplicate benchmark runners, 15+ scratch dirs |

## Right (do not disturb)

Immutable CSC model + reversible presolve stack · sparse-basis LU/eta under simplex ·
`--engine auto` extensibility · independent verifiers · dependency isolation guarded in CI.

## Related

[[07-current-architecture]] (full detail, C.1–C.5) · [[Solve-Pipeline]] · [[08-codebase-audit]] ·
[[12-keep-remove-rebuild]] · [[target]] · [[Architecture MOC]]
