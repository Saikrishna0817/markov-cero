---
type: paper
title: "A Fast LU Factorization for Linear Programming Bases"
authors: "Suhl & Suhl"
year: 1990
venue: "Math. Prog."
doi: "(unverified)"
domain: [sparse, numerics]
priority: ○
status: standard
tags: [paper, sparse, numerics]
---

# A Fast LU Factorization for Linear Programming Bases

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Threshold partial pivoting: LP basis factorization that keeps sparsity without giving up stability.
## Metadata
| Field | Value |
|---|---|
| Authors | Suhl & Suhl |
| Year | 1990 |
| Venue | Math. Prog. |
| DOI/URL | (unverified) |
## Problem Addressed
Full partial pivoting reorders columns and wrecks sparsity on LP bases; no pivoting at all risks tiny pivots and catastrophic rounding on degenerate models. The paper settles the middle ground that production simplex codes actually use.
## Core Contribution
- **Methodology:** Threshold partial pivoting restricted to the current column's maximum, combined with fast sparse storage that exploits the mostly-fixed basis pattern across iterations.
- **Assumptions:** Basis pattern changes slowly (column swaps), so sparse workspaces can be reused; a small threshold gives adequate stability (approximate).
- **Benchmarks/datasets:** LP bases from contemporary collections (qualitative; no numbers asserted).
- **Metrics:** Factorization time, fill, accuracy of computed steps (qualitative).
- **Key results:** Large speedups for basis factorization while preserving numerical quality (approximate; no figures asserted).
## Engineering-Relevant Knowledge
**Algorithms:** Sparse LU factorization of the LP basis with threshold partial pivoting.
**Techniques:** Threshold pivoting (τ·column max rule), trivial-pivot presolve, pattern-reusing sparse storage, in-place factorization workspaces.
**Implementation details:** The concrete recipe for src/linalg/sparse_basis.cpp: preallocate workspace for the basis pattern, eliminate trivial pivots first, only permute when the current entry falls below τ·max — then record τ as an experimentally tuned constant with its own ablation.
**Equations/rules:** Accept pivot aᵢⱼ if |aᵢⱼ| ≥ τ · max_k |aₖⱼ|, else search the column; τ ∈ (0,1] controls the sparsity/stability trade.
**Limitations/failure cases:** τ too small → instability on ill-conditioned models (R13); τ too large → fill and slower solves; interacts with scaling (Oren) before factorization.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** This is the default modern choice for basis factorization, directly serving R4 (simplex), R6 (sparse) and R9 (numerical stability) — and the threshold is a documented tunable our robustness dossier (R17) can report.
## Evidence → Engineering Decision
- *Finding:* Threshold τ trades fill against accuracy and must be tuned per model class → *PS requirement:* R13 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Numerical Error
## Related Papers
- [[Markowitz-1957-Elimination-Form-Inverse]] [[Forrest-1972-Updating-Triangular-Factors]] [[Bartels-1969-Simplex-LU-Decomposition]]
## Uses
- [[Sparse LU]] [[Numerical Stability]]
