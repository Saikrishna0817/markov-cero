---
type: concept
tags: [algorithms, sparse]
status: stable
verified_on: 2026-09-25
---

# Sparse LDL Factorization

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Symbolic then numeric factorization of a symmetric quasi-definite matrix — no pivot search, which is why the QP KKT path can be both sparse and safe.

## Definition
For a symmetric matrix K (here the KKT matrix of a convex QP), an LDLᵀ factorization writes K = L D Lᵀ with unit lower-triangular L and diagonal D, avoiding square roots and, for *quasi-definite* matrices (indefinite but with positive definite principal subblocks), guaranteeing existence without numerical pivoting. It runs in two phases: a symbolic pass that computes the fill pattern from the sparsity graph and ordering, and a numeric pass that computes values — so changing only numerical parameters (ρ, σ) reruns the numeric phase alone. Correctness of the sparse pattern depends on an elimination-tree argument; solves are forward/backward triangular sweeps plus diagonal scaling.

## Why It Matters Here
- R2 (QP) and R6 (sparse numerical linear algebra): the ADMM engine's per-iteration cost is one KKT solve, so this factorization *is* the QP runtime.
- Observed state: Davis Algorithm 849 (`ldl_symbolic` src/qp/kkt.cpp:13, `ldl_numeric` :52, triangular solves :112-134); `KktSolver::factorize` runs symbolic then numeric, `update_numeric` reruns numeric only when ρ/σ change (src/qp/kkt.cpp:226-263).
- Observed state: `perm_`/`pinv_` are "identity if unused" — **no fill-reducing ordering is applied** (include/markov_cero/qp/kkt.hpp:65-67), which is the concrete instantiation of the [[Fill-Reducing Ordering]] gap.

## Key Facts / Rules
- Quasi-definite K = [P+σI, Aᵀ; A, −diag(ρ)⁻¹] with σ, ρ > 0 ⇒ non-singular LDLᵀ without pivot search.
- Symbolic phase result depends only on graph + ordering; numeric phase result depends on values.
- Fill = new nonzeros in L; ordering (AMD/COLAMD) is the primary control lever.
- Two-phase split is what makes adaptive-ρ ADMM affordable: only `update_numeric` runs per ρ change.

## Related
- [[Symbolic Factorization]]
- [[Fill-Reducing Ordering]]
- [[KKT Conditions]]
- [[ADMM]]
- LDL-Factorization

## Referenced By

- Algorithms MOC
- Architecture MOC
- Research MOC
- [[ADMM|research/algorithms/ADMM]]
- research-dependency-map
- [[Karmarkar-1984-New-Polynomial-Time-Algorithm|research/papers/Karmarkar-1984-New-Polynomial-Time-Algorithm]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm|research/papers/Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector|research/papers/Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior|research/papers/Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Vanderbei-1995-Symmetric-Indefinite-Systems|research/papers/Vanderbei-1995-Symmetric-Indefinite-Systems]]
- [[Wright-1997-Primal-Dual-IPM|research/papers/Wright-1997-Primal-Dual-IPM]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods|research/papers/Wright-1997-Primal-Dual-Interior-Point-Methods]]
- [[No Interior-Point Engine|research/research-gaps/No Interior-Point Engine]]
- [[Symbolic Factorization|research/techniques/Symbolic Factorization]]