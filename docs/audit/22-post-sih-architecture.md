# 22. Post-SIH Architectural Dossier: Advanced Extensions & Long-Term Roadmap (P3)

**Document ID**: MARKOV-CERO-P3-ARCH-2026  
**Status**: Target Design Specification  
**Scope**: Post-Qualification Milestone Architecture (P3 Items from `15-roadmap.md`)  
**Mathematical Grounding**: Zhang (2025), Kimiaei (2025), Linan (2025), Canturk (2024)

---

## 1. Executive Summary

This architecture specification formalizes the engineering roadmap for Markov-CERO beyond the Smart India Hackathon (SIH) Grand Finale qualification. While milestones M1 through M5 established a completely sovereign, clean-room C++20 optimization kernel (featuring Revised Primal/Dual Simplex, Steepest-Edge Pricing, Interior Point Barrier methods, First-Order GPU-PDLP, and Parallel Branch-and-Cut MILP/MIQP), the post-SIH roadmap focuses on:
1. **Machine Learning-Assisted Branching & Node Selection** via bipartite graph neural embeddings.
2. **MIQP Convexification & Perspective Reformulation** for semi-continuous and indicator quadratic constraints.
3. **Nonlinear & Mixed-Integer Nonlinear Programming (MINLP)** interfaces via Sequential Quadratic Programming (SQP) and Filter IPM.
4. **Distributed Memory Scalability** using Partitioned Global Address Space (PGAS) and asynchronous work stealing.
5. **Zero-Overhead Algebraic Modeling Layer** providing declarative sets, indices, and parameters without runtime overhead.

---

## 2. ML-Assisted Branching & Learning-to-Search (ED-009)

### 2.1 Theoretical Framework
Modern branch-and-bound relies on Strong Branching (SB) for variable selection to minimize tree size. However, exact SB incurs $O(k \cdot \text{LP-pivots})$ operations at each node, consuming up to 80% of total solver wall-clock time.

Following Zhang et al. (2025) and Kimiaei et al. (2025), the MILP instance is modeled as an undirected bipartite graph $\mathcal{G} = (\mathcal{V}_{c}, \mathcal{V}_{v}, \mathcal{E})$:
- Constraint nodes $i \in \mathcal{V}_c$ represent rows with features: normalized RHS $b_i$, row sense, dual activity, and cosine angle with the objective vector.
- Variable nodes $j \in \mathcal{V}_v$ represent columns with features: objective coefficient $c_j$, fractional part $f_j = x_j^* - \lfloor x_j^* \rfloor$, pseudocost up/down ratios, bounds, and variable type.
- Edge weights $(i, j) \in \mathcal{E}$ correspond to non-zero constraint coefficients $A_{ij}$.

A Graph Convolutional Network (GCN) predicts variable scores $\hat{s}_j \approx \Delta^- z_j \cdot \Delta^+ z_j$, mimicking strong branching scores in a single forward pass:
$$\mathbf{h}_v^{(l+1)} = \sigma \left( \mathbf{W}_v^{(l)} \mathbf{h}_v^{(l)} + \sum_{c \in \mathcal{N}(v)} \alpha_{cv} \mathbf{W}_c^{(l)} \mathbf{h}_c^{(l)} \right)$$

### 2.2 In-Tree Hook Architecture
To preserve clean-room sovereignty (R10) while enabling ML integration, Markov-CERO defines the following pluggable interface in `include/markov_cero/milp/branch_selector.hpp`:

```cpp
namespace markov_cero::milp {

struct NodeFeatureVector {
    std::vector<float> row_features;      // [m x d_row]
    std::vector<float> col_features;      // [n x d_col]
    std::vector<int32_t> edge_indices;   // [2 x nnz]
    std::vector<float> edge_values;       // [nnz]
};

class IBranchingScorer {
public:
    virtual ~IBranchingScorer() = default;
    virtual void score_candidates(
        const NodeFeatureVector& features,
        const std::vector<std::size_t>& fractional_indices,
        std::vector<double>& out_scores) = 0;
};

} // namespace markov_cero::milp
```

### 2.3 Post-SIH Execution Plan
1. **Data Pipeline**: Implement an instrumentation logger in `branch_selector.cpp` to dump training pairs $(\mathcal{G}, s^*)$ to HDF5/Arrow format during strong-branching runs on MIPLIB 2017.
2. **Inference Engine**: Provide an isolated plugin `markov_cero_ml_plugin.so` compiled against an embedded, header-only quantized neural inference runtime (Int8/FP16), eliminating dynamic dependencies on large Python environments.

---

## 3. MIQP Tightening & Perspective Formulations

### 3.1 Mathematical Derivation (Linan et al. 2025)
In Mixed-Integer Quadratic Programs (MIQP) and portfolio/unit commitment models, separable quadratic terms often interact with binary indicator variables $z \in \{0, 1\}$:
$$q_j(x_j, z_j) = \begin{cases} c_j x_j + \frac{1}{2} d_j x_j^2, & \text{if } z_j = 1 \text{ and } 0 \le x_j \le u_j \\ 0, & \text{if } z_j = 0 \text{ and } x_j = 0 \end{cases}$$

The standard Big-M formulation has a weak continuous relaxation. The convex hull of the epigraph is obtained via the **Perspective Function** $\tilde{q}(x, z) = z \, q(x / z)$:
$$\tilde{q}_j(x_j, z_j) = c_j x_j + \frac{d_j x_j^2}{2 z_j}$$

Because $\frac{x_j^2}{z_j}$ is convex for $z_j > 0$, the epigraph $v_j \ge \frac{d_j x_j^2}{2 z_j}$ is linearized dynamically at relaxation points $(x_j^{(k)}, z_j^{(k)})$ via **Perspective Cuts**:
$$v_j \ge d_j x_j^{(k)} x_j - \frac{1}{2} d_j \left(x_j^{(k)}\right)^2 z_j$$

### 3.2 Cut Pool Integration
The perspective cut generator will be integrated into `src/milp/cut_pool.cpp`:
- **Detection**: Scan for indicator relations $0 \le x_j \le u_j z_j$ where $x_j$ has non-zero diagonal in $Q$.
- **Separation**: At fractional node relaxations $(\bar{x}, \bar{z})$, if $v_j < \frac{d_j \bar{x}_j^2}{2 \bar{z}_j} - \epsilon$, generate the supporting hyperplane cut.

---

## 4. Nonlinear & MINLP Extensions (R3)

Markov-CERO's modular design naturally extends to general nonlinear programming (NLP) and mixed-integer nonlinear programming (MINLP):

```
       ┌────────────────────────┐
       │   MINLP Outer Loop     │ (Outer Approximation / LP-NLP B&B)
       └───────────┬────────────┘
                   │
         ┌─────────┴─────────┐
         ▼                   ▼
┌──────────────────┐ ┌──────────────────┐
│  MILP Master LP  │ │  Continuous NLP  │ (Ruiz + KKT Dense/Sparse)
│  (Markov-CERO)   │ │  (Filter SQP/IPM)│
└──────────────────┘ └──────────────────┘
```

1. **Continuous NLP Subproblem**:
   $$\min_{x} f(x) \quad \text{s.t.} \quad g(x) \le 0, \quad A x = b, \quad l \le x \le u$$
   Leverage the existing interior-point barrier solver (`src/lp/interior/ipm.cpp`) and sparse factorization (`src/linalg/sparse_basis.cpp`) to solve the Newton-KKT system:
   $$\begin{bmatrix} H(x, \lambda) + \Sigma_x & A^T & \nabla g(x)^T \\ A & 0 & 0 \\ \nabla g(x) & 0 & -\Sigma_s \end{bmatrix} \begin{bmatrix} \Delta x \\ \Delta y \\ \Delta \lambda \end{bmatrix} = - \begin{bmatrix} r_{\text{dual}} \\ r_{\text{eq}} \\ r_{\text{ineq}} \end{bmatrix}$$
2. **Outer Approximation (Duran & Grossmann)**:
   Linearize convex nonlinear constraints around NLP solutions $x^{(k)}$ to generate cutting planes for the MILP master problem:
   $$g(x^{(k)}) + \nabla g(x^{(k)})^T (x - x^{(k)}) \le 0$$

---

## 5. Distributed Memory Parallelism (PGAS & Work Stealing)

To scale beyond multi-core SMP (which currently achieves 3.68× speedup on 4 threads via RW-2 batch popping), post-SIH architecture introduces Partitioned Global Address Space (PGAS) using MPI-3 RMA or UPC++:

1. **Two-Tier Work Stealing**:
   - **L1 (Local NUMA / Intra-node)**: Lock-free dual work queue with batch stealing (`src/milp/work_queue.cpp`).
   - **L2 (Inter-node / InfiniBand/RoCE)**: Asynchronous non-blocking RMA steal requests when a node's local queue falls below a low-water mark ($\tau_{\text{low}} = 4$ nodes).
2. **Global Incumbent Broadcast**:
   - Asynchronous one-sided atomic updates to a globally exposed incumbent register, instantly pruning subtrees across the entire cluster without collective synchronization barriers.
3. **GPU-Accelerated Cut Separation**:
   - Batch evaluation of 100,000+ knapsack and MIR tableau rows on GPU using dense matrix-matrix primitives (tensor cores), filtering out non-violating cuts before transferring candidates to the CPU node LP.

---

## 6. Zero-Overhead Modeling Layer (R1)

To provide an intuitive API matching modern Python/C++ modeling frameworks while strictly preserving computational performance:
- **C++20 Expression Templates**:
  Compile-time AST generation for linear expressions:
  ```cpp
  auto expr = 2.5 * x[i] + 3.0 * y[j] <= 10.0;
  model.add_constraint(expr);
  ```
  Generates zero intermediate heap allocations, constructing CSR/CSC matrix triplets directly in memory.
- **Python Sovereign Bindings (`pybind11` or `nanobind`)**:
  Expose `markov_cero.Model`, `markov_cero.solve()`, and NumPy/SciPy sparse matrix interfaces with zero-copy buffer protocol support.

---

## 7. Architectural Roadmap Matrix

| Milestone | Capability | Expected Impact | Reference Literature |
|---|---|---|---|
| **v0.6.0** | ML Branching Trace Data Pipeline | Standardized training sets for GNNs | Zhang (2025), Kimiaei (2025) |
| **v0.7.0** | Embedded ONNX/Quantized Scorer | 2–5× tree node reduction on hard MILP | Canturk (2024), ED-009 |
| **v0.8.0** | Perspective Cut Generator | Fast convergence on indicator MIQP | Linan (2025), Frangioni (2006) |
| **v0.9.0** | Filter SQP & MINLP Outer Approx | Expansion to chemical/refinery MINLP | Duran & Grossmann (1986) |
| **v1.0.0** | PGAS Distributed Tree Search | Cluster scaling to 64+ compute nodes | Ralphs et al. (ALPS/CHiPPS) |

---
