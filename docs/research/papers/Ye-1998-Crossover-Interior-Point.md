---
type: paper
title: "Crossover and Interior Point Algorithms for LP"
authors: "Ye; Lustig et al."
year: 1998
venue: "CPAA; Ann. OR"
doi: "(unverified)"
domain: [ipm]
priority: ★
status: deep
tags: [paper, ipm]
---

# Crossover and Interior Point Algorithms for LP

> The missing bridge from an interior-point solution to a basic optimal solution: crossover to a vertex/basis.

## Metadata
| Field | Value |
|---|---|
| Authors | Ye (1998); Lustig, Marsten & Shanno et al. (1995) |
| Year | 1998 |
| Venue | CPAA; Annals of OR |
| DOI/URL | (unverified) |

## Problem Addressed
IPM converges to the analytic center of the optimal face — not a vertex — so it yields no basis, no exactly complementary solution, and no warm start for MIP node LPs. Branch-and-cut needs bases. Crossover converts the interior point into a basic optimal solution.

## Core Contribution
- **Methodology:** Identify (nearly) active constraints at the IPM iterate, drive primal/dual simplex from that point until complementarity is exact; variants: LP-type crossover (pivot to a vertex) and QP-type crossover (minimize distance to interior point subject to optimality).
- **Assumptions:** Optimal face reached to tolerance; an optimal vertex exists (unique/face handling); linear system for identifying the active set is well-conditioned.
- **Benchmarks/datasets:** Netlib LPs solved by IPM then crossed over.
- **Metrics:** Simplex iterations needed in crossover; total time vs. pure simplex.
- **Key results:** Crossover costs a small number of simplex iterations relative to a cold solve (qualitative; modern solvers report crossover as low-single-digit % of total time, (approximate)).

## Engineering-Relevant Knowledge
**Algorithms:** Primal/dual simplex warm-started from an interior point; active-set identification.
**Techniques:** Tolerance-aware complementarity tightening; crossover basis export for MIP warm starts ([[Warm Start]]).
**Implementation details:** markov-cero has both ingredients separately — `src/lp/reference/revised_simplex.cpp` / `src/lp/dual/dual_simplex.cpp` and a PDLP engine — but no crossover code; `reports/crossover_study.csv` is a *scale* crossover study (CPU simplex vs GPU PDLP), not IPM-to-basis crossover.
**Equations/rules:** stop when `x_i s_i <= tol` for all i and basis is non-singular; use Basis crash heuristics on most-active rows.
**Limitations/failure cases:** Degenerate optimal faces produce many tied actives ([[Degeneracy]]); if the IPM stopped early, crossover pivots can cycle or restore infeasibility.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R4 pairs "revised simplex and interior-point methods"; crossover is what makes an IPM output usable inside branch-and-cut (R5) and for basis warm starts (CLI `--save-basis`/`--warm-start` already exist for simplex). Absent today because the IPM itself is absent.

## Evidence → Engineering Decision
- *Finding:* IPM output is not a basis; MIP node solves want bases → *PS requirement:* R4, R5 → *Component:* src/lp/dual/dual_simplex.cpp (crossover driver target) → *Metric:* crossover iterations to optimality; total solve time.
- *Finding:* Repo "crossover" evidence measures engine scale-over, not basis crossover → *PS requirement:* R16 → *Component:* reports/crossover_study.csv (do not mis-cite as crossover proof) → *Metric:* documented in docs/gpu.md.

## Related Papers
- [[Wright-1997-Primal-Dual-Interior-Point-Methods]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]

## Uses
- [[Crossover]]
- [[Basis]]
- [[Warm Start]]
