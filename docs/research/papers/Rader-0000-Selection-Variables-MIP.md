---
type: paper
title: "Selection of Variables in MIP; Sparse/tri-branching"
authors: "Rader & Riesselman; Turner et al."
year: 0000
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ○
status: standard
tags: [paper, branching]
---

# Selection of Variables in MIP; Sparse/tri-branching

> Candidate-set screening and branching on structures sparser than one variable (multi-way / tri-branching).

## Metadata
| Field | Value |
|---|---|
| Authors | Rader & Riesselman; Turner et al. (merged entry in list) |
| Year | **not stated in reference list** — slug uses 0000 placeholder (see manifest) |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Scoring every fractional variable is wasteful: most are irrelevant. And branching on a single variable can be structurally worse than splitting on a sparsity-preserving structure (e.g. partitioning a clique/group into three parts).

## Core Contribution
- **Methodology:** (a) Screen a candidate set from fractional variables before expensive scoring; (b) design branching rules that split on sets/structures (sparse or tri-branching) rather than a single column, exploiting model sparsity.
- **Assumptions:** Fractionality-based screening; knowledge of sparsity pattern/constraint graph.
- **Benchmarks/datasets:** MIP instances (not itemized in list).
- **Metrics:** Candidate-set size, node counts, LP time saved by better splits.
- **Key results:** Screening cuts scoring cost substantially; structured splits can beat single-variable branching while keeping children sparse.

## Engineering-Relevant Knowledge
**Algorithms:** Candidate screening (fractionality threshold), multi-way/sparse branching.
**Techniques:** Keep child LPs sparse (split on variables that share few constraints); tri-branching for categorical variables.
**Implementation details:** Our `src/milp/branch_selector.cpp` should (1) limit candidates by fractionality before scoring, (2) prefer splits that do not densify the basis. Both are cheap and align with our sparsity-first architecture (R6).
**Equations/rules:** Candidate set C = { j : frac(x_j) ≥ ε }; score only C. Split choice should minimize added nonzeros in child matrices.
**Limitations/failure cases:** Screening thresholds are instance-sensitive; multi-way branching complicates node accounting and parallel work distribution.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Candidate screening is a small, sparsity-aligned improvement to code we already have; multi-way branching is a larger design step (R5, R6).

## Evidence → Engineering Decision
- *Finding:* scoring cost scales with #fractional variables → *PS requirement:* R5, R6 → *Component:* src/milp/branch_selector.cpp → *Metric:* LP time per node, node count

## Related Papers
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Turner-0000-Intelligent-Branching-Large]]
- [[Driebeek-1966-Algorithm-Assignment-Problem]]
- [[Held-2006-Lookahead-Branching-Mixed]]

## Uses
- [[Pseudo-Cost Branching]] [[Strong Branching]] [[Branch and Bound]] [[Basis]]
