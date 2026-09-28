---
type: audit
phase: 10
title: Target Architecture (Current → Problems → Target)
tags: [audit, target-architecture, phase-10]
status: complete
date: 2026-09-25
---

# Phase 10 — Target Architecture

## CURRENT ARCHITECTURE
(see `docs/audit/07-current-architecture.md` for verified detail)

```
MPS → canonicalize (dense gate 2048×8192 | sparse) → presolve (3 rules) + Ruiz
  → dispatch in apps/markov_cero_solve.cpp (499-line monolith)
      → primal | dual(Harris) | pdlp(CPU/GPU) | milp(root-only cuts, no stealing) | qp | miqp
  → postsolve → verifiers → JSON
Missing between stages: comparison harness · robustness tools · IPM/crossover
```

## PROBLEMS
(evidence in `docs/audit/07` AP-1..AP-12, `docs/audit/09`, `docs/audit/10`)

1. Evaluation layer absent (R16 hard gap) — no baseline, no profiles, no hardware metadata.
2. Cuts terminate at root → R5 delivers 0.0% node reduction.
3. Parallel search has no load balancing → R7 negative.
4. No IPM/crossover → R4 unmet; no refinement/condition tools → R17 unsupported.
5. Orchestration trapped in the CLI app → R14 "API" is nominal.
6. Two canonicalization paths with an arbitrary threshold → R12 scale risk.
7. GPU story measured wrongly (status bug) and claimed too strongly.
8. Doc/claims drift eroding trust.

## TARGET ARCHITECTURE

```
                    ┌──────────────────────────────────────────────┐
   MPS / LP text ──►│ Model layer (immutable, CSC)                │
                    │  src/model + io (single canonicalization:   │
                    │  sparse-first, dense fast-path is internal) │
                    └───────────────┬──────────────────────────────┘
                                    ▼
                    ┌──────────────────────────────────────────────┐
                    │ Reduction layer                              │
                    │  presolve rules (singleton/doubleton/IM-    │
                    │  bound/probing-lite) + Ruiz scaling          │
                    │  LIFO postsolve ⇄ (unchanged)                │
                    └───────────────┬──────────────────────────────┘
                                    ▼
                    ┌──────────────────────────────────────────────┐
   solve() API ────►│ Engine layer (library, src/api orchestrates) │
   (C++ API + CLI)  │  LP: revised simplex │ dual simplex          │
                    │       (steepest-edge + Harris)               │
                    │       │ PDLP-PDHG (CPU/CUDA)                 │
                    │       │ [R4] sparse IPM + crossover ──NEW──  │
                    │  MILP: branch-and-cut                        │
                    │       ├ in-tree cut loop: GMI+MIR+pool ─NEW─ │
                    │       ├ branching: strong/reliability        │
                    │       ├ heuristics: rounding+FP (+diving P2) │
                    │       └ scheduler: stealing/limits ────NEW── │
                    │  QP: ADMM+LDLᵀ (unchanged) → MIQP           │
                    │  numerics: refinement + condition est ─NEW─  │
                    └───────────────┬──────────────────────────────┘
                                    ▼
                    ┌──────────────────────────────────────────────┐
                    │ Verification layer (unchanged, promoted)     │
                    │  dual-gated verifiers + KKT certificates     │
                    └───────────────┬──────────────────────────────┘
                                    ▼
                    ┌──────────────────────────────────────────────┐
                    │ Evaluation layer ─────────── NEW (P0)        │
                    │  run_compare.py vs HiGHS baseline            │
                    │  suites: Netlib + MIPLIB + Mittelmann + QPLIB │
                    │  hard-instance dossier (degenerate/ill-cond) │
                    │  outputs: table + geom-mean + Dolan–Moré      │
                    │  hardware manifest (CPU/GPU/RAM)             │
                    └───────────────┬──────────────────────────────┘
                                    ▼
                    JSON solution + certificate ──► CLI / C++ API
                                    │
                    Reporting: markdown/CSV for deck + demo video
```

Component mapping (target → existing notes): engine layer ≈ `docs/codebase/components/*`
(kept per `docs/audit/12`); *NEW* items map to RW-1…RW-9; evaluation layer is greenfield.

## Explicit distinctions

| Dimension | Current | Target |
|---|---|---|
| Canonicalization | 2 paths + dense gate | 1 sparse-first path |
| IPM/crossover | absent | minimal sparse IPM + crossover (or documented re-scope) |
| Cuts | root-only | in-tree loop + pool, measured |
| Parallel | shared heap, no stealing | load-balanced scheduler, measured scaling |
| API | orchestration in `main` | `src/api/solve_*` + thin CLIs + install() |
| Evaluation | single-solver CSVs, no baseline | harness vs HiGHS, profiles, dossier, hardware manifest |
| Numerics | tolerances only | + iterative refinement + condition estimation |
| GPU | claimed, losing, status bug | fixed measurement, honest crossover claim |
| Docs | drift | claims↔evidence table, R1–R20 coverage in STATUS |

## Non-goals (favors simplest architecture, rule 11)

- No GUI, no web service, no database, no container/cloud — PS excludes them (R14).
- No ML branching in this cycle ([M16] deferred; P3).
- No NLP/MINLP engines now — extension points only (R3 is judged structurally).
- No rewrite of simplex/ADMM/verifier cores — they pass audit.
