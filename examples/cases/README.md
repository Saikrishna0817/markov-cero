# Industrial Case Studies (Requirement R11)

This directory contains three real-world inspired industrial mathematical programming models formulated in standard MPS / QPS format and solved using the clean-room sovereign **markov-cero** engine.

All models are independently verified by the sovereign KKT and primal/dual feasibility verifier (`src/model/verifier.cpp` and `src/qp/verifier.cpp`).

---

## 1. Multi-Period Production Scheduling (`multiperiod_production.mps`)

### Formulation
A multi-period capacitated lot-sizing problem with setup binaries, inventory holding, and discrete planning periods $t \in \{1, 2, 3\}$.

$$\min \sum_{t=1}^3 \left( c_t^{\text{prod}} x_t + c_t^{\text{hold}} s_t + f_t y_t \right)$$

subject to:
- **Inventory Balance**:
  $$s_{t-1} + x_t - s_t = d_t \quad \forall t \in \{1, 2, 3\} \quad (s_0 = 0)$$
- **Capacity & Setup Coupling**:
  $$x_t \le M_t y_t \quad \forall t \in \{1, 2, 3\}$$
- **Variable Domains**:
  $$x_t \ge 0, \quad s_t \ge 0, \quad y_t \in \{0, 1\}$$

### Solve Output & Verification
- **Engine**: Branch-and-Cut MILP
- **Status**: `Optimal`
- **Verification**: `VERIFIED` (`original primal verified`)
- **Objective Value**: `5300.0`
- **Runtime**: `1.67 ms`
- **Nodes Explored**: `2`
- **Cuts Generated**: `3` (GMI / MIR cuts)
- **Heuristics**: `1` (Feasibility pump)
- **Exit Code**: `0`

---

## 2. Supply Chain Logistics Network (`supply_chain_logistics.mps`)

### Formulation
A capacitated facility location and distribution network with 2 candidate depots/plants $i \in \{1, 2\}$ and 3 customer delivery zones $j \in \{1, 2, 3\}$.

$$\min \sum_{i=1}^2 f_i y_i + \sum_{i=1}^2 \sum_{j=1}^3 c_{ij} x_{ij}$$

subject to:
- **Customer Demand Satisfaction**:
  $$\sum_{i=1}^2 x_{ij} \ge D_j \quad \forall j \in \{1, 2, 3\}$$
- **Depot Capacity Limits**:
  $$\sum_{j=1}^3 x_{ij} \le C_i y_i \quad \forall i \in \{1, 2\}$$
- **Variable Domains**:
  $$x_{ij} \ge 0, \quad y_i \in \{0, 1\}$$

### Solve Output & Verification
- **Engine**: Branch-and-Cut MILP
- **Status**: `Optimal`
- **Verification**: `VERIFIED` (`original primal verified`)
- **Objective Value**: `3770.0`
- **Runtime**: `1.24 ms`
- **Nodes Explored**: `4`
- **Cuts Generated**: `2`
- **Heuristics**: `1`
- **Exit Code**: `0`

---

## 3. Chemical Process Optimization (`crude_oil_blending.mps`)

### Formulation
A continuous convex Quadratic Program (QP) representing refinery crude stream blending to produce a target specification fuel while minimizing blending cost and non-linear octane variance penalty:

$$\min \sum_{i=1}^4 c_i x_i + \frac{1}{2} \sum_{i=1}^4 q_i x_i^2$$

subject to:
- **Total Production Target**:
  $$\sum_{i=1}^4 x_i = V_{\text{target}} \quad (131.0 \text{ Mbbl/d})$$
- **Maximum Sulfur Limit**:
  $$\sum_{i=1}^4 s_i x_i \le S_{\max} \cdot V_{\text{target}}$$
- **Minimum Research Octane Number (RON)**:
  $$\sum_{i=1}^4 r_i x_i \ge R_{\min} \cdot V_{\text{target}}$$
- **Stream Availability**:
  $$0 \le x_i \le U_i \quad \forall i \in \{1, 2, 3, 4\}$$

### Solve Output & Verification
- **Engine**: Convex QP (Sovereign ADMM with Ruiz diagonal scaling)
- **Status**: `Optimal`
- **Verification**: `VERIFIED` (`QP KKT certificate verified`)
- **Objective Value**: `9846.939`
- **Runtime**: `0.28 ms`
- **Iterations**: `275`
- **Primal Solution**: `x = [47.731, 0.000, 62.910, 20.344]`
- **Exit Code**: `0`

---

## Summary Matrix

| Case Study | Problem Class | Rows | Cols | Nonzeros | Status | Verified | Time (ms) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Multi-Period Production** | MILP | 6 | 9 | 14 | Optimal | Yes | 1.67 ms |
| **Supply Chain Logistics** | MILP | 5 | 8 | 14 | Optimal | Yes | 1.24 ms |
| **Crude Oil Blending** | Convex QP | 4 | 4 | 16 | Optimal | Yes | 0.28 ms |

All case studies execute deterministically, produce RFC 8259 JSON outputs, and satisfy independent verification checks.
