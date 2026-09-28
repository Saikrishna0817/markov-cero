---
type: paper
title: "Machine Learning Algorithms for Improving Exact Classical Solvers in Mixed Integer Continuous Optimization"
authors: "Kimiaei, Kungurtsev & Olimba"
year: 2025
venue: "arXiv"
doi: "(unverified)"
domain: [ml]
priority: ✦
status: standard
tags: [paper, ml]
---
# Machine Learning Algorithms for Improving Exact Classical Solvers in Mixed Integer Continuous Optimization
> Survey of where ML can attach to an exact MIP solver — branching, cut selection, heuristics, tuning — with the data and evaluation requirements each attachment implies.
## Metadata
| Field | Value |
|---|---|
| Authors | Kimiaei, Kungurtsev & Olimba |
| Year | 2025 |
| Venue | arXiv |
| DOI/URL | https://arxiv.org/abs/2508.06906 |

## Problem Addressed
"ML for solvers" is scattered across dozens of component-level papers with inconsistent evaluation. The survey organizes integration points inside an exact solver, the learning paradigms used (imitation, ranking, reinforcement, GNNs), and what evidence is needed to claim a component actually improved.
## Core Contribution
- **Methodology:** Structured review of ML integration levels: presolve choice, cut selection/scoring, branching-variable selection, primal heuristics, hyperparameter control; discussion of datasets, baselines and evaluation pitfalls.
- **Assumptions:** Classical solver remains the source of optimality guarantees; ML only guides it.
- **Benchmarks/datasets:** MIP collections used across the ML-for-optimization literature (not re-verified).
- **Metrics:** Node-count/time reduction at fixed settings, gap closed, generalization across instance sizes.
- **Key results:** Component-level ML gains often fail to generalize across distributions; rigorous evaluation with classical baselines is the recurring requirement (qualitative restatement).
## Engineering-Relevant Knowledge
**Algorithms:** ML-guided branch-and-cut components (selection/ranking models).
**Techniques:** Imitation learning from solver logs, learned scoring of candidate cuts, distributional evaluation discipline.
**Implementation details:** Our components (branch_selector, cut_pool, heuristics) have the logs a future learner would need, but PS excludes ML pipelines; the survey's *evaluation discipline* (baselines, generalization) is still useful for classical tuning.
**Limitations/failure cases:** Data collection and training infrastructure cost; distribution shift across instance families.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** PS explicitly excludes ML from required scope; the value here is the checklist for evaluating any future learned component, not a deliverable.
## Evidence → Engineering Decision
- *Finding:* No ML component is in scope; classical component tuning is the actual gap → *PS requirement:* R5 → *Component:* src/milp/branch_selector.cpp → *Metric:* node count reduction from classical branching policies (ML deferred)
## Related Papers
- [[Unknown-2025-Apollo-MILP-Alternating]]
- [[Canturk-2024-Scalable-Primal-Heuristics]]
## Uses
- [[Cut Efficiency]]
