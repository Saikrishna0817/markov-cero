---
type: paper
title: "The Anti-Cycling Rule for the Simplex Method"
authors: "Bland"
year: 1977
venue: "Operations Research"
doi: "(unverified)"
domain: [lp]
priority: ★
status: deep
tags: [paper, lp]
---

# The Anti-Cycling Rule for the Simplex Method

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Bland's rule: the simple tie-breaking rule that guarantees the simplex terminates — at a price.

## Metadata
| Field | Value |
|---|---|
| Authors | Bland |
| Year | 1977 |
| Venue | Operations Research |
| DOI/URL | (unverified) |

## Problem Addressed
The simplex can cycle forever on degenerate LPs — visiting the same bases repeatedly without improving the objective — and for years this was an open theoretical embarrassment with no known fix that preserved practical behavior. Bland supplies the first provably finite rule.

## Core Contribution
- **Methodology:** Tie-breaking discipline: always choose the smallest-index column among admissible entering candidates (and a consistent leaving-row rule), which prevents any basis from ever repeating, hence guarantees finite termination.
- **Assumptions:** Finite candidate sets; rule only alters choices among ties (approximate).
- **Benchmarks/datasets:** Constructed cycling examples (Beale) and degenerate LPs (qualitative).
- **Metrics:** Termination guarantee (theoretical); iteration counts (empirical).
- **Key results:** First finite-termination proof for the simplex under degeneracy — a theoretical result of enormous practical importance as a safety net (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Simplex with Bland anti-cycling fallback active only when a stall/cycle is suspected.

**Techniques:** Smallest-index tie-breaking, stalling detection that switches rules, hybrid policies (Bland ↔ normal pricing).

**Implementation details:** Our src/lp/reference/revised_simplex.cpp currently relies on Bland-style anti-cycling as the default guarantee; the lesson from later work (Gill's EXPAND, Koberstein's tolerances) is that Bland should be the *fallback*, not the primary rule, because strict tie-breaking destroys pivot quality (approximate).

**Equations/rules:** Entering column = min{j : c̄ⱼ < 0} under Bland (with consistent leaving-row choice); no basis repeats ⇒ termination (Bland Anti-Cycling).

**Limitations/failure cases:** Pathologically slow on degenerate models — iteration counts can explode; unsuitable as the permanent pricing rule in a production solver (approximate).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R9 requires reliable convergence and R13 targets degenerate models; Bland is the cheap correctness floor every from-scratch simplex must have, and the literature tells us exactly how to go beyond it without losing the guarantee.

## Evidence → Engineering Decision
- *Finding:* A guaranteed-finite fallback must exist before any aggressive pricing/tolerance policy → *PS requirement:* R9 → *Component:* src/lp/reference/revised_simplex.cpp → *Metric:* KKT Residual
- *Finding:* Strict Bland pricing is too slow to be the primary rule → *PS requirement:* R20 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* Geometric Mean Runtime

## Related Papers
- [[Gill-1989-Practical-Anti-Cycling]]
- [[Harris-1973-Pivot-Selection-Methods]]
- [[Koberstein-2005-Dual-Simplex-Method]]
- [[Maros-2003-Generalized-Dual-Phase]]

## Uses
- [[Bland Anti-Cycling]] [[Degeneracy]] [[Revised Simplex]]
