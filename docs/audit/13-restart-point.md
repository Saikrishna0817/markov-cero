---
type: audit
phase: 9
title: Restart Point — Dependency-Aware Sequence
tags: [audit, restart-point, phase-9]
status: complete
date: 2026-09-25
---

# Phase 9 — The Correct Restart Point

## The answer

> **Restart at the evaluation layer, not at the core.**
> First component to build: **`scripts/run_compare.py` — the external-solver comparison
> harness (R16)** — *then* the fixes it will immediately measure (in-tree cuts, parallel
> scheduling), *then* interior-point/crossover (R4) and the robustness dossier (R17).

Why this point is correct, by dependency:

1. **It is the only hard PS requirement that is 100% missing** (R16, `docs/audit/09` §6.1)
   and it is *evaluation*, not implementation — meaning every subsequent code change needs
   it to prove value. Building algorithm work first repeats the existing failure mode
   (6 phases of code, no baseline).
2. **It gates credibility.** All current performance claims are unverifiable without a
   baseline ([D] K1/K2). The harness is also the measuring instrument for RW-1 (cuts),
   RW-2 (parallel), RW-9 (GPU) — you cannot manage those without it.
3. **It is deadline-feasible**: HiGHS installs from wheels/apt, is open-source (PS explicitly
   allows comparing against open-source), and is *not* linked into the solver —
   `check-sovereignty.py` stays untouched (comparing ≠ building upon; C1 preserved).
4. **It is not blocked by anything** — MPS files, runners and CI already exist.

## Freeze + sequence (dependency order)

| Step | Action | Depends on | Unblocks | Effort |
|---|---|---|---|---|
| 0 | **Freeze current code** (tag `v0.5.2-audit-baseline`); record hardware inventory (CPU/GPU/RAM) into `evidence/hardware.md` | — | reproducibility (PS-GAP-05) | hours |
| 1 | **Build comparison harness** `scripts/run_compare.py` + baseline install (HiGHS); emit table + geometric means + Dolan–Moré profile for Netlib+MIPLIB shared sets | step 0 | R16 ✅, E1, all later measurements | 1 day |
| 2 | **Claims audit pass**: fix README/STATUS/evidence claims (GPU 13/13, parallel 0.56×, test counts); fix `run_gpu.py:241-245` status bug | step 1 | credibility (K2), honest demo | 0.5 day |
| 3 | **In-tree cut loop (RW-1)**: re-cut at nodes, pool reuse, measure cut-node reduction vs step-1 baseline | step 1 | R5 proof | 1-2 days |
| 4 | **Parallel load balancing (RW-2)**: work stealing or conservative `--threads 1` default; re-measure scaling | step 1 | R7 honest claim | 1-2 days (or 0.5h to demote) |
| 5 | **Robustness dossier (R17)**: curated degenerate/ill-conditioned/weak-relaxation set + report generator (can start with iterative refinement RW-8) | step 1 | R13/R17 demo, slide 5 | 1-2 days |
| 6 | **Interior-point decision** (R4): implement minimal sparse Mehrotra-style IPM + crossover on small/medium LPs **or** formal re-scope with evidence; whichever chosen, document in STATUS | step 1 (compare first to know where IPM matters) | R4 | 2-4 days (the long pole) |
| 7 | **Demo packaging**: `run-qualification-demo.sh` v2 (steps 1+2+5 wired), video script, 6-slide PDF | 1-5 | SIH deliverables | 1 day |
| 8 | P1 backlog (steepest-edge, presolve depth, API/RW-4, sparse-first RW-5, Mittelmann R15) | 1 | grade improvement | post-deadline |

## Why not start somewhere else?

- *Not at "rewrite the core"*: core is sound + tested; rewrite ⇒ deadline miss (rule 10/11).
- *Not at "build more algorithms first"*: reproduces Phase 0 finding — implementation ahead
  of evidence.
- *Not at "frontend/API polish"*: PS explicitly deprioritizes interface (R14), and without
  R16/R17 there's nothing to wrap.
- *Not at GPU*: GPU currently *loses*; fixing the measurement (step 1-2) precedes GPU work.

**Inference:** steps 0–7 fit 5 days if parallelized across 2-3 people (harness+dossier ‖
cuts+parallel ‖ slides+video); step 6 (IPM) is the only risk item — decide on day 1.
