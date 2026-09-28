---
type: paper
title: "Apollo-MILP: An Alternating Prediction-Correction Neural Solving Framework for MILP"
authors: "(authors not stated in source list)"
year: 2025
venue: "ICLR"
doi: "(unverified)"
domain: [ml]
priority: ✦
status: standard
tags: [paper, ml]
---
# Apollo-MILP: An Alternating Prediction-Correction Neural Solving Framework for MILP
> Neural prediction alternating with optimization-based correction to warm-start MILP solving — the ML track the PS explicitly places outside required scope.
## Metadata
| Field | Value |
|---|---|
| Authors | Not stated in the source list (verify: ICLR 2025) |
| Year | 2025 |
| Venue | ICLR |
| DOI/URL | https://iclr.cc/virtual/2025/poster/30706 — note: same URL is listed for entry 139 (TRGNN) in the source list; at least one link is wrong |

## Problem Addressed
MILP solvers spend early search time discovering incumbent-quality assignments. The paper asks whether a learned model can predict promising solutions/variable states and an optimization step can repair them, alternating until the classical solver takes over.
## Core Contribution
- **Methodology:** Alternating prediction-correction loop: neural network predicts (initial solution / variable values), a correction step enforces optimization-consistency, iterate; predicted solution used to warm-start a classical MIP solver.
- **Assumptions:** Training instances drawn from the same distribution as test instances; classical solver accepts a start (MIP start).
- **Benchmarks/datasets:** Standard MILP instance collections used in ML-for-MIP work (not re-verified).
- **Metrics:** Time to first incumbent, primal bound at limit, gap closed versus cold start.
- **Key results:** Reported gains in primal performance from predicted warm starts (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Neural prediction + optimization correction hybrid solving.
**Techniques:** Warm-starting a classical solver with predicted solutions; alternating refinement instead of one-shot prediction.
**Implementation details:** Would plug into `src/milp/heuristics.cpp` via a start-solution injection point — but requires a training pipeline, which the PS explicitly excludes from required scope.
**Limitations/failure cases:** Distribution shift kills learned predictors; adds ML infrastructure the PS does not ask for.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** The PS states no ML/AI model or training pipeline is required (explicit "NOT required" list) — this is a future/optional track (Module 16), not part of R1–R20 delivery.
## Evidence → Engineering Decision
- *Finding:* PS excludes ML from required scope while our primal heuristics are unmeasured → *PS requirement:* R5 → *Component:* src/milp/heuristics.cpp → *Metric:* time to first incumbent with classical heuristics (ML deferred)
## Related Papers
- [[Kimiaei-2025-Machine-Learning-Algorithms]]
- [[Canturk-2024-Scalable-Primal-Heuristics]]
## Uses
- [[Warm Start]]
