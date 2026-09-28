---
type: paper
title: "A Novel Linear Optimization Presolve Technique Based on Fourier-Motzkin Elimination"
authors: "Zhang, Ploskas & Sahinidis"
year: 2026
venue: "Math. Prog. Comp."
doi: "10.1007/s12532-026-00278-w (derived from list link)"
domain: [presolve]
priority: ✦
status: standard
tags: [paper, presolve]
---
# A Novel Linear Optimization Presolve Technique Based on Fourier-Motzkin Elimination
> FME-based presolve with a predictor of how many reductions each elimination will yield; 6-11% CPU reductions on CPLEX (per source list).
## Metadata
| Field | Value |
|---|---|
| Authors | Zhang, Ploskas & Sahinidis |
| Year | 2026 |
| Venue | Math. Prog. Comp. |
| DOI/URL | 10.1007/s12532-026-00278-w |
## Problem Addressed
Classical presolve rules are local and hand-designed; Fourier-Motzkin elimination is complete but explodes combinatorially. The paper revives FME as a targeted presolve step, using reduction-size estimates to choose eliminations that pay off.
## Core Contribution
- **Methodology:** Select variables to eliminate by FM projection, guided by an estimate of resulting constraint reduction; keep only eliminations projected to shrink the model; integrate as optional presolve pass.
- **Assumptions:** Linear model; coefficient-tolerance control to avoid coefficient blow-up; budget on projection work.
- **Benchmarks/datasets:** Benchmarks through CPLEX (per list: 6-11% CPU reduction) — numbers as reported in source list, treat as (approximate) for our stack.
- **Metrics:** CPU time, model size reduction.
- **Key results:** 6-11% CPU reductions on CPLEX (as stated in source list).
## Engineering-Relevant Knowledge
**Algorithms:** Bounded Fourier-Motzkin projection with reduction forecasting.
**Techniques:** Elimination ordering by predicted payoff; coefficient management.
**Implementation details:** A generalization path for our presolve beyond fixed rules; can be run selectively when the model is small after initial reductions.
**Limitations/failure cases:** FM triples row counts when badly chosen — the estimator is the safety; coefficient growth hurts [[Ill-Conditioning]].
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R5 (presolve) with R9 (numerics): an optional, budgeted pass fits the existing `max_passes` design (`PresolveOptions::max_passes`).
## Evidence → Engineering Decision
- *Finding:* Estimating reduction size before eliminating avoids presolve blow-up → *PS requirement:* R5, R9 → *Component:* src/presolve/presolve.cpp (optional FM pass with budget) → *Metric:* presolve time vs. rows removed ratio.
## Related Papers
- [[Andersen-1995-Presolving-Linear-Programming]]
- [[Achterberg-2020-Presolve-Reductions-Mixed]]
## Uses
- [[Presolve]]
- [[Sparsity]]
