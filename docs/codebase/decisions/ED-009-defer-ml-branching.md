---
id: ED-009
title: Defer Machine-Learning Branching Engine to Post-SIH
status: Accepted
date: 2026-09-26
tags: [architecture, milp, branching, machine-learning, post-sih]
---

# Architectural Decision Record ED-009: Defer ML-Assisted Branching to Post-SIH

## Context
Recent mathematical literature in mixed-integer linear programming (e.g., Kimiaei et al. 2025, Zhang et al. 2025, Canturk et al. 2024) explores machine learning (ML) for variable selection, node selection, and diving heuristics. Learning-to-branch algorithms (e.g., GCNs on bipartite variable-constraint graphs) attempt to imitate Strong Branching at a fraction of the computational expense.

However, incorporating an ML runtime directly into the core C++20 solver for the SIH evaluation introduces severe architectural risks:
1. **Clean-room Sovereignty (R10)**: Linking heavy third-party inference engines (PyTorch C++ LibTorch, ONNXRuntime, or TensorFlow C API) introduces massive dynamic dependency graphs, violating sovereign deployment and standalone static compilation requirements.
2. **Inference Latency Overhead**: For medium-scale instances (<5,000 nodes), ML model forward-pass latency (often 5–20 ms on CPU per evaluation) is 100× slower than dual steepest-edge simplex re-solve pivots (0.01–0.05 ms). Without GPU batching or quantized linear models, GNN branching is net-negative on wall-clock time.
3. **Data Pipeline Prerequisite**: Supervised imitation learning requires generating hundreds of gigabytes of exact strong-branching traces across diverse MIPLIB instances to train robust graph neural network models.

## Decision
1. **Scope Boundary**: Full deep-learning branching inference is explicitly deferred to post-SIH development (Roadmap item P3).
2. **In-Tree Substrate (Completed in P1/P2)**:
   - Provide high-performance analytical heuristics: Reliability Branching (`reliability`), Pseudo-cost branching (`pseudo_cost`), and Strong Branching (`strong_branching`) with lookahead budgets.
   - Design a modular `IBranchScoreOracle` hook in `include/markov_cero/milp/branch_selector.hpp` allowing external scorers to score fractional candidates.
3. **Training Data Instrumentation**: Markov-CERO provides feature-extraction interfaces (bipartite variable-constraint degree, LP reduced costs, fractional distances, pseudocost history) suitable for offline trace dumping.

## Consequences
- **Positive**: The solver remains a zero-dependency, statically linkable, sovereign C++20 binary with predictable microsecond-level node execution.
- **Positive**: Compliance with evaluation criterion R10 and zero third-party framework fragility.
- **Negative**: Deep learning neural node selection is not evaluated during SIH live qualification (mitigated by competitive exact strong branching and reliability branching).

## References
- Kimiaei, M. et al. (2025). *Machine Learning in Mixed-Integer Programming: Survey and Perspectives*.
- Zhang, H. et al. (2025). *Learning to Select Nodes in Branch-and-Bound via Bipartite Graph Embeddings*.
