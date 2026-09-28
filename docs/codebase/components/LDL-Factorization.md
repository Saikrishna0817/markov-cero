---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/qp/kkt.cpp:13"
  - "include/markov_cero/qp/kkt.hpp:15"
verified_on: 2026-09-25
---

# LDL-Factorization

> Davis sparse LDLᵀ (Algorithm 849) used to factorize the symmetric quasi-definite KKT matrix.

## Responsibility
- Provide symbolic/numeric sparse LDLᵀ for the augmented KKT system `[P+σI, Aᵀ; A, −diag(ρ)⁻¹]`, plus fast numeric re-factorization when ρ/σ change.

## Implementation Facts (Observed)
- Declared in include/markov_cero/qp/kkt.hpp:11-17, which states the system form and: "Uses Davis's sparse LDL^T factorization (Algorithm 849). Because the system is ... (SQD), non-singular LDL^T factorization is guaranteed without numerical pivot searching."
- Stages in src/qp/kkt.cpp: `ldl_symbolic` (src/qp/kkt.cpp:13), `ldl_numeric` (src/qp/kkt.cpp:52), triangular solves `ldl_lsolve` / `ldl_dsolve` / `ldl_ltsolve` (src/qp/kkt.cpp:112, 126, 134).
- `KktSolver::factorize` builds the KKT sparsity and runs symbolic then numeric (src/qp/kkt.cpp:226, 240); `update_numeric` re-runs only the numeric phase for a new ρ/σ (include/markov_cero/qp/kkt.hpp:30-33, src/qp/kkt.cpp:263).
- Storage: upper-triangular CSC of K (`kkt_col_ptr_/row_ind_/val_`) and factors `L_col_ptr_/L_row_ind_/L_val_`, `D_`, `parent_` (include/markov_cero/qp/kkt.hpp:53-63).
- Permutation fields `perm_/pinv_` are "identity if unused" — no ordering (AMD/COLAMD) applied (include/markov_cero/qp/kkt.hpp:65-67).
- Solve applies forward L, diagonal D, backward Lᵀ (src/qp/kkt.cpp:287-293).
- A separate *dense* LDLᵀ-style check exists for convexity screening in src/qp/model.cpp:162-205 (different code path).

## Dependencies
- [[SparseBasis-LU]] (host `SparseCsc` type only), [[QP-ADMM-Engine]]

## Used By
- [[QP-ADMM-Engine]] (every ADMM iteration); `qp`/`miqp` engines in [[Solve-Pipeline]]

## Research Justification
- [[Interior-Point Method]] (KKT systems), [[ADMM]] (inner linear solve)

## Open Questions / Risks
- No fill-reducing ordering: Inference: large/sparse KKT matrices may suffer high fill before `nonzeros_L()` becomes large (no cap observed in src/qp/kkt.cpp).
- The header's SQD guarantee depends on σ>0 and ρ>0; enforcement of those input conditions is in [[QP-ADMM-Engine]] options (UNVERIFIED edge cases).
