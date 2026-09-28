---
type: paper
title: "A Combined Linear and Nonlinear Presolve for Nonlinear Optimization"
authors: "Zhang & Sahinidis"
year: 2025
venue: "EURO J. Comput. Optim."
doi: "10.1016/j.ejco.2025.100119"
domain: [presolve]
priority: ✦
status: standard
tags: [paper, presolve]
---
# A Combined Linear and Nonlinear Presolve for Nonlinear Optimization
> Unifies linear presolve machinery with nonlinear reductions — the presolve roadmap for a future NLP/MINLP extension.
## Metadata
| Field | Value |
|---|---|
| Authors | Zhang & Sahinidis |
| Year | 2025 |
| Venue | EURO J. Comput. Optim. |
| DOI/URL | 10.1016/j.ejco.2025.100119 |
## Problem Addressed
NLP solvers bolt ad-hoc simplifications onto linear presolve, producing inconsistent tolerances and untracked transformations. The paper builds one framework where linear and nonlinear reductions coexist with shared validity tracking.
## Core Contribution
- **Methodology:** Extends linear presolve (bounds, singleton, redundancy) with nonlinear rules (fixed-variable substitution in nonlinear terms, trivial equation simplification, convexity-aware bound propagation), all reversible.
- **Assumptions:** Smooth NLP/MINLP; conservative tolerances; transformation stack shared across rule types.
- **Benchmarks/datasets:** MINLP/NLP collections (paper's set).
- **Metrics:** Reductions, solve time, correctness of postsolve.
- **Key results:** Combined presolve outperforms linear-only on MINLP models (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Combined linear/nonlinear presolve pipeline.
**Techniques:** Reversible transformation stack generalized to nonlinear terms.
**Implementation details:** Our `PresolveStack` (include/markov_cero/presolve/presolve.hpp) is linear-only; its LIFO record design (code: pop reduction records in reverse) is the right skeleton to generalize per R3.
**Limitations/failure cases:** Nonlinear bound propagation is expensive; incorrect tolerances corrupt solutions (R17 risk).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R3 (extensible to NLP/MINLP later) — this defines how presolve should be structured *now* so the NLP extension does not require a rewrite.
## Evidence → Engineering Decision
- *Finding:* A reversible, rule-agnostic transformation stack survives domain extensions → *PS requirement:* R3, R5 → *Component:* include/markov_cero/presolve/presolve_stack.hpp (keep records self-contained per rule) → *Metric:* postsolve correctness (verifier pass rate).
## Related Papers
- [[Zhang-2026-Novel-Linear-Optimization]]
- [[Achterberg-2020-Presolve-Reductions-Mixed]]
## Uses
- [[Presolve]]
- [[Numerical Stability]]
