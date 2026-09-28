---
type: paper
title: "Presolving in Linear Programming"
authors: "Andersen & Andersen"
year: 1995
venue: "Math. Prog."
doi: "10.1007/BF01586000"
domain: [presolve]
priority: ★
status: deep
tags: [paper, presolve]
---

# Presolving in Linear Programming

> Canonical LP presolve survey: singleton/doubleton rows and columns, implied bounds, dominated/forcing constraints, plus dual recovery in postsolve.

## Metadata
| Field | Value |
|---|---|
| Authors | Andersen & Andersen |
| Year | 1995 |
| Venue | Math. Prog. |
| DOI/URL | 10.1007/BF01586000 |

## Problem Addressed
Presolve rules were scattered across codes with no unified treatment of *correctness* — especially how duals must be restored after primal-only reductions. The paper gives a systematic catalogue with the tolerances needed for safe application and a principled postsolve (dual recovery).

## Core Contribution
- **Methodology:** Rules grouped as row/column singletons, doubleton rows/cols, implied bounds from bound propagation, dominated and forcing constraints; each rule carries a validity tolerance; transformations recorded and inverted at postsolve with duals reconstructed (singleton rows propagate dual values, etc.).
- **Assumptions:** LP in standard/bounded form; tolerance-consistent bound arithmetic; reversible transformation stack.
- **Benchmarks/datasets:** Netlib LPs (measured reductions in rows/cols and time).
- **Metrics:** Rows/columns/nonzeros removed; simplex iterations before/after; wall time.
- **Key results:** Large models shrink substantially; solve time drops accordingly (qualitative; per-instance numbers in paper).

## Engineering-Relevant Knowledge
**Algorithms:** LP presolve + postsolve with dual recovery.
**Techniques:** Bound propagation (implied bounds), dominated/forcing detection, singleton elimination.
**Implementation details:** Our implementation (`src/presolve/presolve.cpp`) has only empty-row, empty-column, row-singleton and fixed-variable rules with a reversible stack (`include/markov_cero/presolve/presolve.hpp`) — doubletons, implied bounds and dominated rows are absent. Postsolve already recomputes duals for singletons (code comment at row-singleton substitution), so the Andersen dual-recovery pattern extends naturally.
**Equations/rules:** implied bound from row: `l_j = max(l_j, (b_i - sum_{k!=j} a_ik * bound_k)/a_ij)` with direction by sign of `a_ij`; apply only if violation <= tolerance.
**Limitations/failure cases:** Tolerance too tight -> no reductions; too loose -> infeasible after postsolve ([[Numerical Stability]]); redundant-row tests interact with [[Degeneracy]].

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 names presolve, R20 wants fewer iterations/time on industrial models. This is the taxonomy our 4-rule presolve should be extended against, rule by rule, with correctness evidence per rule.

## Evidence → Engineering Decision
- *Finding:* Andersen rules 2-5 (doubleton, implied bounds, dominated, forcing) are missing from our presolve → *PS requirement:* R5 → *Component:* src/presolve/presolve.cpp + include/markov_cero/presolve/presolve.hpp → *Metric:* presolved rows/cols reduction %; Netlib iterations before/after (evidence/netlib_results.csv).
- *Finding:* Every primal reduction needs a dual recovery formula → *PS requirement:* R9, R17 → *Component:* postsolve path in src/presolve/presolve.cpp → *Metric:* dual feasibility violation after postsolve (verifier).

## Related Papers
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques]]
- [[Achterberg-2020-Presolve-Reductions-Mixed]]
- [[Gondzio-1997-Presolve-Analysis-LPs]]
- [[Brearley-1975-Analysis-Mathematical-Programming]]

## Uses
- [[Presolve]]
- [[Reduced Cost]]
- [[Degeneracy]]
