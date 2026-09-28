---
type: paper
title: "Updating Triangular Factors of the Basis to Maintain Sparsity"
authors: "Forrest & Tomlin"
year: 1972
venue: "Math. Prog."
doi: "(unverified)"
domain: [sparse, lp]
priority: ★
status: deep
tags: [paper, sparse, lp]
---

# Updating Triangular Factors of the Basis to Maintain Sparsity

> Keeps L,U current through cheap rank-1 updates between refactorizations instead of refactoring every iteration.

## Metadata
| Field | Value |
|---|---|
| Authors | Forrest & Tomlin |
| Year | 1972 |
| Venue | Math. Prog. |
| DOI/URL | (unverified) |

## Problem Addressed
Refactorizing the basis from scratch at every simplex iteration is wasteful, yet naively updating an inverse or factors lets fill-in and rounding error grow without bound. The paper gives the standard middle path: apply structured elementary updates to the triangular factors while their quality holds, then refactorize on a measured trigger.

## Core Contribution
- **Methodology:** Rank-1 (column replacement) updates applied to L and U by elimination-style operations, preserving sparsity as long as possible, with refactorization scheduled rather than per-iteration.
- **Assumptions:** Successive bases differ by one column swap; accuracy of updated factors degrades gradually so a threshold can detect it (approximate).
- **Benchmarks/datasets:** LPs of the early 1970s (qualitative; no numbers asserted).
- **Metrics:** Time per iteration, nnz growth of L,U, residual of updated factorization (qualitative).
- **Key results:** Updating is substantially cheaper than refactoring over typical iteration runs, making it the default simplex basis strategy for decades (approximate; no figures asserted).

## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex with factor updates between refactorizations; dual simplex basis maintenance.

**Techniques:** Forrest–Tomlin elementary operations, fill/error monitoring, refactorization trigger policy, symbolic reuse of the original sparsity pattern.

**Implementation details:** src/linalg/sparse_basis.cpp needs both paths — an update path for cheap iterations and a refactor path — plus an explicit trigger (max updates, fill growth, residual check); the trigger constants must be measured on Netlib/MIPLIB rather than guessed.

**Equations/rules:** Column replacement ΔB = (a_new − a_old)e_kᵀ handled as a rank-1 update; refactor when nnz(L) exceeds ρ·nnz(L₀) or ‖B − LU‖/‖B‖ exceeds tolerance (Numerical Error).

**Limitations/failure cases:** Fill and accumulated rounding eventually destroy the update path; degenerate/poorly ordered bases make updates unstable fast (approximate) — without a residual check, drift is silent.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R4/R6 require efficient basis maintenance at every iteration and at every MIP node; the refactor trigger is a measurable policy that directly trades runtime against stability (R9, R20) and is not yet evidence-tuned in our code.

## Evidence → Engineering Decision
- *Finding:* Update-vs-refactor is a tunable policy, not a fixed rule → *PS requirement:* R9 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime
- *Finding:* Silent factorization drift requires periodic residual verification → *PS requirement:* R17 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* KKT Residual

## Related Papers
- [[Bartels-1969-Simplex-LU-Decomposition]]
- [[Reid-1982-Sparsity-Exploiting-Variant]]
- [[Suhl-1990-Fast-LU-Factorization]]
- [[Curtis-1972-Simplex-LU-Decomposition]]

## Uses
- [[Basis]]
- [[Sparse LU]]
- [[Numerical Stability]]
