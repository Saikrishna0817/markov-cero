---
type: competitive-note
tags: [competitive, reference, established-solvers, r16]
status: verified
date: 2026-09-25
---

# Established solvers

> The reference column for R16 ("compared against at least one established commercial or
> open-source solver") — and the standard our rivals are already measured against.

## Capability table [Observed: official docs + Mittelmann tables]

| Solver | LP engines | MILP depth | QP | GPU | Licence | Mittelmann standing |
|---|---|---|---|---|---|---|
| **HiGHS** | simplex pr+du, IPX/HiPO **IPM + crossover**, HiPDLP | branch-and-cut, strong presolve, multithreaded (2026) | convex QP, **no MIQP** | cuPDLP-C + HiPDLP (v1.10+) | MIT | **best open source**: LPfeas 17.2, LPopt 13.07, MILP 7.36 vs COPT 1.00 |
| **SCIP** | SoPlex simplex (exact rational; IPM via plugins) | **deepest**: branch-cut-and-price, PaPILO presolve, plugins, UG parallel | via nonlinear handlers | no | Apache-2.0 (since 8.0.3) | MILP 9.93 (SCIPC 8.45) |
| **CBC/CLP** | simplex (+ rudimentary IPM) | rich CGL cut set (Gomory, MIR, cover, clique, odd-wheel, zero-half) | no native QP | no | EPL-2.0 | ~an order behind HiGHS |
| **GLPK** | simplex ("dummy" IPM) | weak cuts/heuristics, serial | no | no | **GPL-3.0 copyleft** | bottom tier |
| **CPLEX / Gurobi / Xpress** | all three engines | production depth, parallel | full MIQP | Gurobi/Xpress PDLP GPU | commercial | **withdrawn from public rankings** (IBM/FICO removal demands; Gurobi exited 2024) |

## Why they beat any student solver today

- HiGHS alone is ~10 years of hyper-sparsity-aware LU + dual-simplex refinement + tuned
  branch-and-cut. The from-scratch C++23 solver **mipx** measures itself at **2–5× slower on
  medium Netlib, 10–40× on the hardest, 2–8× on easy MIPLIB** vs HiGHS
  [Observed: github.com/spoorendonk/mipx] — the honest student band.
- That band is exactly where the rivals publish: SANKHYA 2.62×, Deekshith-snapshot 2.57×,
  team-vertexx 62% of HiGHS, VX03 13–350×, IGAOS simplex 10–100×.

## What a sovereign student solver can honestly claim instead

1. **Licence + dependency freedom**: GLPK's copyleft blocks proprietary embedders; our
   Apache-2.0 + zero-solver-dependency core (CI-enforced) is a real sovereignty argument.
2. **Auditability**: decades-old codebases vs a clean-room core with math→code traceability
   ([[21-traceability]], 204-reference corpus) and independent verifiers.
3. **Domain fit**: Indian refinery/blending/planning case models (R11) that incumbents don't ship.
4. **Published, re-runnable scorecards** at small-to-medium scale — not speed, not scale.

## Benchmark-rules facts worth copying

- **Pinned baseline with verbatim flags** (IGAOS `benchmark-protocol.md`: HiGHS v1.15.1,
  `--presolve=on --parallel=off --threads=1 --random_seed=0`, shifted geometric mean,
  fair-comparison §9) — best-written protocol in the field; we should match it in
  `evidence/compare/report.md`.
- **Separate-process comparison + ldd sovereignty gate** (SANKHYA `ci.yml:808-816`).
- **Instance sha256 + git commit + machine tag stamped on every CSV row** (SANKHYA generator).

Related: [[19-competitive-landscape]] §19.3 · [[Prior art - solver and GPU projects]]

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]
- [[Prior art - solver and GPU projects|competitive/Prior art - solver and GPU projects]]