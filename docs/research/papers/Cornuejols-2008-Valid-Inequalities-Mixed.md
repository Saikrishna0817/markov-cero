---
type: paper
title: "Valid Inequalities for Mixed Integer Linear Sets"
authors: "Cornuéjols"
year: 2008
venue: "Math. Prog. B"
doi: "(unverified)"
domain: [milp]
priority: ★
status: deep
tags: [paper, milp]
---

# Valid Inequalities for Mixed Integer Linear Sets

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Unified treatment of split, GMI, lift-and-project and intersection cuts and the relationships between them.

## Metadata
| Field | Value |
|---|---|
| Authors | Cornuéjols |
| Year | 2008 |
| Venue | Math. Prog. B |
| DOI/URL | (unverified) |

## Problem Addressed
Cut families were developed separately with separate theory; practitioners could not tell which are stronger, which imply which, or how they relate to Chvatal-Gomory rank. The survey establishes the hierarchy and the geometry (split closure, disjunctive cuts).

## Core Contribution
- **Methodology:** Derives split cuts, GMI cuts, lift-and-project, intersection cuts from a common disjunctive framework; shows implication relations and closure properties; ties cut strength to formulation geometry.
- **Assumptions:** MILP set `S = {x in P: x_I integer}`; validity means cutting no point of S; separation may be heuristic.
- **Benchmarks/datasets:** Expository examples (polyhedral figures, small sets).
- **Metrics:** Cut strength (closure containment); Chvatal rank.
- **Key results:** Split closure ⊆ closure relationships; CG cuts and split cuts coincide in rank for 0-1 sets (qualitative summary of the theory).

## Engineering-Relevant Knowledge
**Algorithms:** Split/GMI/lift-and-project/intersection cut generation.
**Techniques:** Disjunctive derivation; validity checking; strength ordering.
**Implementation details:** We generate GMI (`src/milp/gomory.cpp`) and MIR (`src/milp/mir.cpp`, which is a split-derived family) — this paper explains why MIR/GMI behave similarly and when aggregation changes validity. Cut filtering today is cosine-based; validity theory would justify tighter filters.
**Equations/rules:** a cut is valid iff `a^T x <= b` for all x in S; split cut from floor/ceil dichotomy on a fractional variable.
**Limitations/failure cases:** Separation heuristics may miss violated cuts; numerically rounded cuts can become invalid ([[Cut Validity]] vs. floating point).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 (cutting planes) and R17 (trustworthy results): every cut emitted must be provably valid — the theory gives the checklist and the tolerance caveats.

## Evidence → Engineering Decision
- *Finding:* MIR/GMI are split-family cuts; validity depends on coefficient rounding → *PS requirement:* R5, R9 → *Component:* src/milp/mir.cpp + src/milp/gomory.cpp (validity assertions before pool insertion) → *Metric:* [[Cut Efficiency]]; verifier-detected invalid-cut count (should be zero).
- *Finding:* Stronger closures justify heavier separation effort → *PS requirement:* R5 → *Component:* src/milp/cut_pool.cpp → *Metric:* root gap closed per cut round.

## Related Papers
- [[Achterberg-2005-General-Mixed-Integer]]
- [[Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Wolsey-1989-Strong-Formulations-Mixed]]

## Uses
- [[Cut Validity]]
- [[Gomory Mixed Integer Cut]]
- [[Mixed Integer Rounding Cut]]
- [[LP Relaxation]]
