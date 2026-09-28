---
type: paper
title: "Rounding Errors in Algebraic Processes (1963); Accuracy and Stability of Numerical Algorithms (1996/2002)"
authors: "Wilkinson; Higham"
year: 1963
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ★
status: deep
tags: [paper, numerics]
---

# Rounding Errors in Algebraic Processes (1963); Accuracy and Stability of Numerical Algorithms (1996/2002)

> The two books that define backward stability and componentwise error bounds — the theory against which every solver routine is judged.

## Metadata
| Field | Value |
|---|---|
| Authors | Wilkinson (1963); Higham (1996/2002) — merged entry in list |
| Year | 1963 (first); 1996/2002 for Higham |
| Venue | (books; not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Why do algorithms that are mathematically exact fail in floating point, and how do you *prove* a computed result is close to the true one? Without a stability framework, "it worked on my test set" is the only evidence available.

## Core Contribution
- **Methodology:** Wilkinson: backward error analysis for Gaussian elimination/pivoting — the computed solution is the exact solution of a slightly perturbed problem. Higham: modern componentwise bounds, condition-number-based accuracy prediction, and a systematic framework (forward/backward/perturbation analysis).
- **Assumptions:** Model of IEEE floating-point arithmetic; norms and conditioning as the yardstick.
- **Benchmarks/datasets:** Standard numerical examples and test matrices.
- **Metrics:** Forward/backward error; condition × machine-ε accuracy estimates.
- **Key results:** The vocabulary of [[Numerical Stability]]: backward stable ⇒ accurate when κ is moderate; accuracy = κ · ε. Every claim in our ADR-M0-03 numerical policy traces here.

## Engineering-Relevant Knowledge
**Algorithms:** Backward error analysis; error-free transformations; condition-based accuracy estimates.
**Techniques:** Report errors as κ(A)·ε rather than absolute thresholds; prefer backward-stable formulations (LU with partial pivoting) over clever but unstable ones.
**Implementation details:** Use as the citation basis for tolerance choices in `src/lp/dual/dual_simplex.cpp` and for the numerical claims in `docs/decisions/ADR-M0-03-numerical-policy.md`; supports the R17 robustness dossier.
**Equations/rules:** ‖x̂ − x‖ / ‖x‖ ≲ κ(A) · c_n · ε_mach (standard model ‖ΔA‖ ≤ c_n ε ‖A‖).
**Limitations/failure cases:** Bounds can be pessimistic; κ itself must be estimated (#144); backward stability of the *whole* simplex (many updates) does not follow from stability of one solve — see #150/#151.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Supplies the theoretical spine for R9/R17: we can state, with a citable bound, what accuracy our LP/QP answers achieve.

## Evidence → Engineering Decision
- *Finding:* tolerance/accuracy claims need a formal basis → *PS requirement:* R9, R17 → *Component:* src/lp/dual/dual_simplex.cpp, docs/decisions/ADR-M0-03-numerical-policy.md → *Metric:* [[Numerical Error]], [[KKT Residual]]

## Related Papers
- [[Cline-1979-Estimate-Condition-Number]]
- [[Moler-1967-Rounding-Errors-Algebraic]]
- [[Gleixner-2015-Iterative-Refinement-Linear]]
- [[Bartels-1968-Numerical-Investigation-Simplex]]

## Uses
- [[Numerical Stability]] [[Ill-Conditioning]] [[Iterative Refinement]] [[KKT Residual]]
