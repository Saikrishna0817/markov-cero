---
type: paper
title: "Scalable Primal Heuristics Using Graph Neural Networks for Combinatorial Optimization"
authors: "Cantürk, Varol, Aydoğan & Özener"
year: 2024
venue: "Journal of Artificial Intelligence Research 80:327–376"
doi: "10.1613/jair.1.14972"
domain: [ml]
priority: ✦
status: standard
tags: [paper, ml]
---
# Scalable Primal Heuristics Using Graph Neural Networks for Combinatorial Optimization
> GNN primal heuristics with inductive bias that transfer across instance sizes — learned feasibility finding as a complement (not a replacement) for classical heuristics.
## Metadata
| Field | Value |
|---|---|
| Authors | Cantürk, Varol, Aydoğan & Özener |
| Year | 2024 |
| Venue | Journal of Artificial Intelligence Research 80:327–376 |
| DOI/URL | 10.1613/jair.1.14972 |

## Problem Addressed
Classical primal heuristics are hand-designed per problem class and degrade when instance size changes. The paper asks whether a graph neural network trained on feasible solutions can generalize (inductively) to unseen and larger instances and produce feasible candidates quickly.
## Core Contribution
- **Methodology:** Represent combinatorial instances as graphs, train a GNN to predict solution components with problem-specific inductive bias, decode predictions to candidate solutions, and repair/verify them with a classical feasibility check.
- **Assumptions:** Training instances share structure with test instances; candidates are verified by an exact feasibility test before use.
- **Benchmarks/datasets:** Combinatorial optimization collections (TSP/network-flow style sets; not re-verified).
- **Metrics:** Feasibility rate of predicted solutions, solution quality vs. classical heuristics, runtime per candidate, generalization to larger sizes.
- **Key results:** Inductive GNN heuristics can transfer across sizes and produce feasible solutions cheaply when biased correctly (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** GNN-based primal heuristics (prediction + decode + repair).
**Techniques:** Inductive architectures that do not fix input size; candidate repair loop; always verify with an exact checker.
**Implementation details:** The *repair + verify* pattern mirrors what our `src/verify/primal_verifier.cpp` already does for classical candidates; ML itself is out of required scope (PS "NOT required" list).
**Limitations/failure cases:** Predicted candidates are frequently infeasible without repair; requires training data and infrastructure excluded by the PS.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Primal quality matters for R5/R20, but the PS excludes ML pipelines; the applicable lesson is verification discipline for any heuristic candidate, learned or classical.
## Evidence → Engineering Decision
- *Finding:* Primal heuristics exist but their time-to-first-feasible is unmeasured → *PS requirement:* R5 → *Component:* src/milp/heuristics.cpp → *Metric:* feasibility success rate and time to first incumbent
## Related Papers
- [[Unknown-2025-Apollo-MILP-Alternating]]
- [[Kimiaei-2025-Machine-Learning-Algorithms]]
## Uses
- [[Warm Start]]
