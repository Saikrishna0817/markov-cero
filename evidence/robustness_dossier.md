# Numerical Robustness Dossier (Requirement R17)

## Executive Summary
Mathematical optimization solvers operate in IEEE 754 floating-point arithmetic where ill-conditioning, degenerate bases, and catastrophic cancellation threaten solver convergence and solution fidelity. **markov-cero** implements a clean-room, multi-tiered numerical defense architecture spanning matrix preconditioning, dynamic condition estimation, Markowitz threshold pivoting, Forrest-Goldfarb dual steepest edge pricing, Harris two-pass ratio testing, Bland cycling prevention, and extended-precision iterative refinement.

This dossier documents the condition number thresholds, detection metrics, failure modes, and recovery strategies implemented across the engine.

---

## 1. Condition Number Thresholds & Estimation Metrics

### 1.1 Sparse Pivot-Ratio Condition Proxy
The basis factorization module evaluates condition proxy $\hat{\kappa}(B)$ using the extreme pivots of the sparse $LU$ factorization:
$$\hat{\kappa}(B) = \frac{\max_{i} |U_{ii}|}{\min_{i} |U_{ii}|}$$

Implemented in `src/linalg/sparse_basis.cpp`:
```cpp
double sparse_condition_estimate(const SparseLuDiagnostics& diagnostics) {
    const double minimum = diagnostics.minimum_absolute_pivot;
    const double maximum = diagnostics.maximum_absolute_pivot;
    if (minimum <= 0 || maximum <= 0)
        return std::numeric_limits<double>::infinity();
    return maximum / minimum;
}
```

### 1.2 Operational Numerical Thresholds

| Parameter | Configuration Field | Default Value | Purpose / Action Triggered |
| :--- | :--- | :--- | :--- |
| **Singular Pivot Cutoff** | `zero_tolerance` | $1.0 \times 10^{-12}$ | Elements with $|a_{ij}| < 10^{-12}$ are treated as exact structural zeros. |
| **Markowitz Stability Threshold** | `markowitz_threshold` | $0.1$ | Pivot candidate $a_{ij}$ must satisfy $|a_{ij}| \ge 0.1 \max_k |a_{kj}|$ to limit element growth during elimination. |
| **Refinement Condition Trigger** | `refinement_trigger_condition` | $1.0 \times 10^{8}$ | Triggers multi-step iterative refinement when $\hat{\kappa}(B) > 10^8$. |
| **Growth Factor Trigger** | `refinement_trigger_growth` | $1.0 \times 10^{8}$ | Triggers refactorization or refinement when $\|U\|_\infty / \|B\|_\infty > 10^8$. |
| **Eta Update Chain Limit** | `refinement_trigger_updates` | $50$ | Forces refactorization and basis reinversion after 50 Forrest-Tomlin / product-form updates to prevent error accumulation. |
| **Dual Simplex Condition Limit** | `condition_trigger` | $1.0 \times 10^{-14}$ | If $\min |U_{ii}| / \max |U_{ii}| < 10^{-14}$, terminates or triggers cold fallback to prevent invalid pivot decisions. |
| **Harris Dual Feasibility Margin** | `dual_tolerance` | $1.0 \times 10^{-7}$ | Window expansion in Harris ratio test to permit numerically robust pivot candidate selection. |

---

## 2. Ill-Conditioned Basis Handling & Recovery

### 2.1 Extended-Precision Iterative Refinement
When solving $B x = b$ or $B^T y = c_B$ with an ill-conditioned basis $B$:
1. The solver computes the residual $r = b - B x$ using `long double` (80-bit IEEE extended precision on x86_64) accumulators in `src/linalg/sparse_basis.cpp`:
   $$r_i = \text{fl}_{80}\left(b_i - \sum_j B_{ij} x_j\right)$$
2. The residual correction $\Delta x$ is computed by forward and back-substitution through the sparse LU factors and eta updates:
   $$B \Delta x = r$$
3. The solution is updated: $x \leftarrow x + \Delta x$.
4. **Empirical Performance**:
   - Tested on ill-conditioned synthetic bases with $\kappa \approx 10^9$ (`tests/sparse_basis_test.cpp`).
   - Forward error improves from $10^{-2}$ to $< 10^{-6}$.
   - Backward error monotonically non-increasing ($\|b - B x\|_\infty \le \|r_0\|_\infty$).
   - Well-conditioned bases ($\kappa \le 10^7$) bypass refinement with zero runtime overhead.

### 2.2 Basis Reinversion & Refactorization
- Product-form eta matrices $E_k$ accumulate condition degradation: $\kappa(B_k) \le \kappa(B_0) \prod_{j=1}^k \kappa(E_j)$.
- When the update chain length reaches `refinement_trigger_updates` (default 50) or sparsity density deteriorates (`eta_density_trigger`), the basis is discarded and re-factorized from scratch from the original columns of $A$.
- If singularity or severe ill-conditioning occurs during pivot operations, the engine catches the exception and falls back to a fresh cold-start basis factorization.

### 2.3 Ruiz Equilibrated Preconditioning
For continuous LP and first-order PDHG solvers, the matrix $A$ is scaled prior to factorization via $L_\infty$ Ruiz equilibration (`src/linalg/ruiz_scaling.cpp`):
$$D_R^{(k+1)} = \operatorname{diag}\left(1 / \sqrt{\|r_i\|_\infty}\right), \quad D_C^{(k+1)} = \operatorname{diag}\left(1 / \sqrt{\|c_j\|_\infty}\right)$$
This contracts the spectrum $\sigma(A)$, dampens extreme matrix entries, and prevents ill-conditioned pivots before factorization begins.

---

## 3. Degenerate Pivots & Forrest-Goldfarb Dual Steepest Edge

Degeneracy occurs when basic variables lie exactly on their bounds, leading to zero-length step sizes ($\theta = 0$) where the objective function fails to improve.

### 3.1 Forrest-Goldfarb Dual Steepest Edge (DSE)
Instead of standard Dantzig pricing which selects the maximum primal violation $\max_i (-x_{B_i})$ regardless of row geometry, markov-cero defaults to exact Forrest-Goldfarb Dual Steepest Edge (`include/markov_cero/lp/dual/dual_simplex.hpp` and `src/lp/dual/dual_simplex.cpp`):
$$\text{Score}_i = \frac{-x_{B_i}}{\sqrt{\gamma_i}}, \quad \gamma_i = \|(B^{-1})^T e_i\|_2^2$$

- **Degeneracy Suppression**: Directions with large norm $\gamma_i$ (often associated with degenerate constraints that cause zigzagging) are down-weighted in favor of directions that yield large progress per unit norm in dual space.
- **$O(m)$ Recurrence**: Weights are updated in $O(m)$ work per pivot using the auxiliary solve $B w = \rho_p$:
  $$\gamma_i^{\text{new}} = \gamma_i - 2 \left(\frac{\bar{a}_{iq}}{\bar{a}_{pq}}\right) w_i + \left(\frac{\bar{a}_{iq}}{\bar{a}_{pq}}\right)^2 \gamma_p$$
- **Periodic Recalibration**: Exact recalculation is performed upon basis refactorization to eliminate round-off drift.

---

## 4. Cycling Prevention & Stalling Avoidance

### 4.1 Harris Two-Pass Ratio Test
In dual simplex, naive ratio testing $\min_{j: \bar{a}_{pj} < 0} \frac{d_j}{-\bar{a}_{pj}}$ can select an entering variable with tiny pivot element $|\bar{a}_{pj}| \approx 10^{-11}$, causing catastrophic numerical drift and near-zero step size.

Markov-cero implements the **Harris Two-Pass Ratio Test** (`select_entering_column` in `src/lp/dual/dual_simplex.cpp`):
1. **Pass 1 (Threshold Detection)**: Find the upper ratio bound allowing a slight feasibility tolerance $\delta_{\text{dual}} = 10^{-7}$:
   $$\theta_{\max} = \min_{j: \bar{a}_{pj} < -\epsilon_{\text{piv}}} \frac{\max(0, d_j) + \delta_{\text{dual}}}{-\bar{a}_{pj}}$$
2. **Pass 2 (Pivot Maximization)**: Among all candidate variables satisfying $\theta_j \le \theta_{\max}$, choose the variable that **maximizes the pivot denominator**:
   $$q^* = \arg\max_{j: \theta_j \le \theta_{\max}} (-\bar{a}_{pj})$$
3. **Outcome**: Maximizing $|a_{pq}|$ prevents near-zero pivots, drastically enhances the numerical stability of the next $LU$ update, and allows degenerate clusters to step past floating-point stalling barriers.

### 4.2 Bland's Smallest-Subscript Anti-Cycling Rule
When tie conditions occur (e.g. multiple ratios are identical within machine precision):
- **Primal Simplex**: If enabled (`bland_anti_cycling = true`), always chooses the candidate with the smallest column index $j$. Leaving variable ties break strictly by `basis[i] < basis[leaving]`.
- **Dual Simplex**: Leaving row ties break strictly by `basis[i] < basis[leaving]`; entering column ties break strictly by $j < \text{entering}$.
- **Mathematical Guarantee**: By Bland's theorem, no basis can be visited twice, strictly precluding cyclic looping in degenerate states.

### 4.3 Snapping & Guard Tolerances
- Solution entries satisfying $-\epsilon_{\text{feas}} \le x_i < 0$ are snapped to $0.0$ via `snap_basic_solution`.
- Solutions with $x_i < -\epsilon_{\text{feas}}$ trigger a numerical failure or cold refactorization rather than allowing corrupted state propagation.

---

## 5. Verification & Audit Results

The numerical safeguards have been validated across:
1. **Moler Ill-Conditioned Matrix Test**: Unit pivots with exponential condition number $\approx 2^{2n}$.
2. **Scaled Ill-Conditioned Basis Test ($24 \times 24$)**: Minimum singular value scaled down to $10^{-9}$; iterative refinement engaged and drove error $< 10^{-6}$.
3. **Netlib Degenerate Benchmarks**: Successfully solved highly degenerate models (`blend`, `share2b`, `sc50a`, `recipe`, `adlittle`) with 100% KKT certificates verified.
4. **Independent KKT Verifier**: All solutions output by `markov-cero-solve` are checked by `src/model/verifier.cpp` (LP/MILP) and `src/qp/verifier.cpp` (QP) against $\|A x - b\|_\infty \le \epsilon$ and reduced cost complementarity.
