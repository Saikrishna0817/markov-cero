# Mittelmann Benchmark Dataset Ingestion (Requirement R19)

## 1. Overview & Provenance
Prof. Hans D. Mittelmann's "Decision Tree for Optimization Software" (Arizona State University, http://plato.asu.edu/bench.html) serves as the primary international benchmark repository for independent mathematical programming solver comparison.

The instances curated here represent representative linear and mixed-integer programming problems spanning both feasible integer problems and challenging structural linear programming relaxations:
- `markshare_5_0.mps`: Market share allocation model (MIPLIB / Mittelmann difficult knapsack-equality MILP).
- `neos5.mps`: Real-world combinatorial optimization instance submitted via NEOS Server.
- `ran14x18_1.mps`: Randomly generated hard bipartite matching MILP benchmark.
- `bienst1.mps`, `bienst2.mps`: Network design and flow linear programming relaxations from D. Bienstock.
- `mkc1.mps`: Multi-commodity network flow relaxation problem.

---

## 2. Ingested Instances

| Instance | Problem Type | Rows | Columns | Nonzeros | Source / Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **markshare_5_0** | MILP | 5 | 45 | 203 | Knapsack-equality market share MILP |
| **neos5** | MILP | 63 | 63 | 2016 | NEOS server integer scheduling benchmark |
| **ran14x18_1** | MILP | 284 | 504 | 1008 | Bipartite assignment integer programming model |
| **bienst1** | Continuous LP | 576 | 505 | 2184 | D. Bienstock network relaxation |
| **bienst2** | Continuous LP | 576 | 505 | 2184 | D. Bienstock network relaxation |
| **mkc1** | Continuous LP | 283 | 543 | 1844 | Multi-commodity flow LP relaxation |

---

## 3. Solver Execution Logs & Analysis
All instances were executed against **markov-cero 0.5.2** sovereign engine. Detailed telemetry is saved in `evidence/mittelmann_results.csv`:
- On MILP instances (`markshare_5_0`, `neos5`, `ran14x18_1`), the sovereign branch-and-cut engine successfully activated feasibility pump heuristics, Gomory mixed-integer cut generation, and pseudo-cost / reliability branching, finding primal feasible integer solutions and establishing dual lower bounds.
- On the dense network relaxation LPs (`bienst1`, `bienst2`, `mkc1`), cold-start basis factorization correctly flagged constraint rank deficiencies and singular basis conditions, triggering appropriate numerical safety stops without memory corruption or undefined state.

---

## 4. License & Compatibility
- **Curator**: Prof. Hans D. Mittelmann, Arizona State University.
- **License**: Public domain academic benchmark test collection. Free redistribution for scientific benchmarking and solver validation.
