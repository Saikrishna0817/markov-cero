---
type: paper
title: "Learning to Select Nodes in Branch and Bound with Sufficient Tree Representation"
authors: "Zhang, Zeng, Li, Wu & Li"
year: 2025
venue: "ICLR"
doi: "(unverified)"
domain: [branching]
priority: ✦
status: standard
tags: [paper, branching]
---

# Learning to Select Nodes in Branch and Bound with Sufficient Tree Representation

> TRGNN: represent the search tree as a tripartite graph (nodes, variables, constraints) and learn node selection with a GNN — ICLR 2025.

## Metadata
| Field | Value |
|---|---|
| Authors | Zhang, Zeng, Li, Wu & Li |
| Year | 2025 |
| Venue | ICLR 2025 |
| DOI/URL | (unverified; ICLR virtual link in list) |

## Problem Addressed
Node selection (best-bound vs. best-estimate) uses one scalar per node; the tree's structure, variable states and constraint states are discarded. Can a learned model over the full tree state pick better nodes?

## Core Contribution
- **Methodology:** Build a tripartite heterogeneous graph of B&B nodes × variables × constraints, encode it with a GNN, and train a policy to rank open nodes (RL/supervised selection), replacing scalar bound ordering.
- **Assumptions:** Training instances resembling deployment; inference fast enough not to dominate node time; deterministic requirements met.
- **Benchmarks/datasets:** Standard MIP benchmarks (ICLR 2025 study).
- **Metrics:** Node counts, total time, selection overhead.
- **Key results:** Learned selection can beat classical policies in-distribution — but requires training data and adds a model to a from-scratch solver.

## Engineering-Relevant Knowledge
**Algorithms:** GNN/RL node selection; graph encoding of the search tree.
**Techniques:** Tripartite graph construction; learned ranking with fallback to best-bound.
**Implementation details:** PS explicitly does **not** require ML; our first need is to *implement and report* a classical policy (`src/milp/work_queue.cpp`). Note also audit: Phase 7 "ML-assisted branching" is a deferred repo plan, not a PS requirement.
**Equations/rules:** none applicable.
**Limitations/failure cases:** Off-distribution instances regress; training cost; nondeterminism risk (problem for reproducible benchmarking, R16).

## Applicability to Our Project
**Classification:** Not applicable
**Why:** Out of PS scope (no ML required), and classical node selection is not yet implemented/measured — learn it only after the baseline exists.

## Evidence → Engineering Decision
- *Finding:* classical node selection not yet reported → *PS requirement:* R5, R16 → *Component:* src/milp/work_queue.cpp → *Metric:* time-to-incumbent, [[Geometric Mean Runtime]]

## Related Papers
- [[Achterberg-2007-Best-Estimate-Bound]]
- [[Linderoth-2000-Impact-Branch-Bound]]
- [[Giallombardo-2025-Machine-Learning-Techniques]]
- [[Schweizer-0000-Restart-Strategies-MIP]]

## Uses
- [[Branch and Bound]] [[Pseudo-Cost Branching]] [[Geometric Mean Runtime]] [[MIPLIB]]
