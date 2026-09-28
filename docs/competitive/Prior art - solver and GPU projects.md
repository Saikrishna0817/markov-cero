---
type: competitive-note
tags: [competitive, reference, prior-art, gpu]
status: verified
date: 2026-09-25
---

# Prior art — solver and GPU projects

> Non-SIH landscape: what already exists that our (or any rival's) claims stand next to.
> Cite as prior art; never claim to beat it without same-machine numbers.

## GPU first-order LP

| Project | What it is | Relation to a student GPU claim |
|---|---|---|
| **cuPDLP-C** | C++ GPU PDLP, builds *against HiGHS*, now inside COPT 7.1; "close to commercial" perf | The bar IGAOS measured itself against — and lost 12–27× pre-fix |
| **cuPDLP.jl / FirstOrderLp.jl** | Julia GPU restarted PDHG | Original reference implementation |
| **cuPDLPx** (MIT Lu Lab) | Halpern-PDHG + PID weights, 2.5–6.8× over cuPDLP; CUDA + ROCm | State of the art moves quarterly |
| **NVIDIA cuOpt** | GPU PDLP + barrier (cuDSS) + CPU dual simplex; MILP beta — README admits "proving optimality remains under active development" | **Never claim fastest GPU**; the honest angle is "GPU PDHG from scratch with a published CPU/GPU crossover point" |
| Mittelmann GPU LPfeas column | Dedicated GPU ranking exists (INFORMS 2025) | Evaluator may know it |

## QP / ADMM

- **cuOSQP** (Oxford Control): CUDA ADMM for convex QP — prior art for any student ADMM QP
  engine (ours included); QP-only, so "one binary covering LP/MILP/QP" remains unoccupied.

## Indian ecosystem (sovereignty context)

- **IIT Bombay Minotaur** — open-source MINLP with Argonne; IIT Delhi RL-for-branching work
  on it [Observed]. The nearest thing to an Indian academic solver core.
- **FOSSEE Scilab Optimization Toolbox** — wraps COIN-OR (CLP/CBC/IPOPT): *not* an
  indigenous core, so not a competitor to the PS's sovereignty goal.
- **DRDO / government indigenous solver initiative** — [Not found] in this survey.
- 2026 NIET patent on hardware-accelerated ADMM/PDHG learning-based optimization — adjacent,
  not a solver core.
- **Past SIH solver submissions with published results**: only the IGAOS/igaos project found
  [Not found: others].

## From-scratch student precedents

- **mipx** (C++23 B&C): publishes HiGHS-relative scores (2–40× slower) — the template for
  honest self-positioning (see [[Established solvers]]).
- JAG954/optimization (simplex + Mehrotra IPM, Gurobi cross-checked), CHOP (B&B + Gomory
  with node-count harness), pedagogical tableau repos (gilp, Simplex-from-Scratch).
- None are Indian; none are SIH.

## Honest differentiation angles this prior art leaves open

1. **Scope GPU claims to first-order LP + publish the crossover point** (nobody owns "honest
   student GPU numbers"; cuOpt/cuPDLP own "fast").
2. **Dependency-free permissive core** (GLPK copyleft, SCIP/HiGHS decades-old; cuOSQP QP-only).
3. **Beaten bar, published per-instance**: IGAOS's committed CSVs (100/114 etc.) are a
   checkable target — beat the *artifacts*, not the README.
4. **Complete optimality-certificate pipeline** — cuOpt itself says MIP proof is unfinished;
   a from-scratch presolve→Gomory→B&B→certified gap on small/medium MIPLIB is claimable.
5. **One sovereign binary across LP/MILP/QP at small-to-medium scale** — HiGHS has no MIQP,
   cuOSQP is QP-only, GLPK has no QP.

Related: [[19-competitive-landscape]] §19.4 · [[Established solvers]]

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]
- [[Established solvers|competitive/Established solvers]]