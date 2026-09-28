# GPU Acceleration Architecture & Verification (Phase 5)

> **Hardware verification status (2026-09-28):** Full CUDA builds and GPU PDLP solves were
> executed on a physical NVIDIA GeForce RTX 2050 (sm_86). The four measured generated cases
> (14,400–640,000 parsed nonzeros) passed solution verification, but GPU PDLP was slower
> end-to-end than CPU PDLP in every single run. The one-run-per-case study does not establish
> a crossover threshold. The QP ADMM x-update remains CPU-side; the device computes only the
> `P*x` contribution. See `evidence/benchmarks/crossover_study_gpu_rtx2050.csv` and
> `evidence/gpu_hardware_rtx2050.json`.

Two additional host-level RTX 2050 runs were collected on 2026-09-28, each covering
the same four generated scales in opposite orders. All eight GPU solves passed
verification and the GPU remained slower end to end (about 2.3–8.3x slower than CPU
PDLP). These runs used the existing CUDA-enabled binary with SHA-256
`93c96173cbb918c20658d238012bb0abb8c4f6b492e1c02a540e0a44a3037a5a`; the exact
commands, hardware, and results are in `evidence/gpu_hardware_host_access_check*`
and `evidence/benchmarks/gpu_host_access_check*`. The host NVIDIA driver is healthy;
the task sandbox hides the device. The prior temporary CUDA toolkit is absent, so
this binary was not rebuilt from the current worktree. These repeats do not establish
a performance benefit or replace source-current validation.

---

## 1. Current Status (RTX 2050 PDLP validation complete; QP path and portability open)

- **Solver Core**: Sovereign C++20 core with optional CUDA (`MARKOV_CERO_ENABLE_CUDA`).
- **External Dependencies**: Strict `{C++20 stdlib, Threads, CUDA}`. Zero third-party links.
- **Telemetry Disclosure**: All JSON outputs emit:
  `"limitations":"Sovereign LP/MILP/QP/MIQP (CPU/GPU) engine."`
- **Execution CLI**: PDLP is selectable via `--engine pdlp --backend gpu` with CPU fallback.
- **Measured performance**: No GPU end-to-end benefit was observed on the four RTX 2050 cases;
  do not advertise a speedup or infer a universal crossover from this sample.
- **GPU QP**: The ADMM device kernel computes `P*x`/residual terms, while the linear-system
  solve for the x-update remains on CPU; this is not a GPU-accelerated QP ADMM iteration.

---

## 2. Why Simplex is the Wrong GPU Target

The problem statement asks for "GPU-Accelerated" optimization. In linear programming, attempting
to port the Revised Simplex algorithm directly to GPUs is a well-known anti-pattern in the
optimization literature:

1. **Sequential Dependencies**: Simplex is inherently serial. Each basis change requires
   pivoting, basis update, and pricing. One pivot depends entirely on the previous basis.
2. **Irregular Sparse Triangular Solves**: Computing $B^{-1} a_j$ (FTRAN) and $B^{-T} c_B$ (BTRAN)
   involves irregular pointer-chasing through sparse LU factorizations and Eta matrices. These
   suffer from severe thread divergence and poor memory coalescence on SIMT architectures.
3. **Data Transfer Bottleneck**: Re-uploading basis matrices or transferring pivots between
   host and device dominates runtime, erasing any compute advantage.
4. **Literature Consensus**: Research (e.g., Hall 2010, Bixby 2012) demonstrates that modern
   CPU cache hierarchies and SIMD vectorization outperform GPU simplex implementations on
   standard benchmarks.

---

## 3. Why PDLP (First-Order Primal-Dual) is the Right GPU Target

Primal-Dual Hybrid Gradient (PDHG / PDLP; Applegate et al., NeurIPS 2021; Lu & Yang, cuPDLP.jl,
2023) fundamentally alters GPU feasibility:

1. **Regular, High-Throughput Compute**: Every PDHG iteration consists strictly of:
   - Sparse matrix-vector product (SpMV): $q = A x$
   - Transpose sparse matrix-vector product: $s = A^T y$
   - Elementwise vector axpy, scaling, and bound projections
   - Deterministic parallel reductions (inner products, 2-norms, $\infty$-norms)
2. **Zero In-Loop Memory Transfers**: The matrix $A$ is uploaded once to device memory in CSR
   and CSR-of-$A^T$ formats. All iterate vectors ($x, y, \bar{x}, \bar{y}$) remain device-resident.
   The host receives only scalar convergence telemetry per check interval.
3. **Designed CPU On-Ramp**: `markov-cero` already implements matrix-free PDLP on CPU
   (`src/lp/first_order/pdlp.cpp`). Phase 5 does not invent a new algorithm; it ports this
   validated iteration kernel to CUDA.

---

## 4. Phase 5 Architecture & Decisions

Phase 5 introduces optional GPU acceleration with zero disruption to the non-GPU build:

- **`D-GPU-01` (Target)**: Restarted PDHG, not simplex or branch-and-bound. Avoids serial
  bottlenecks and leverages streaming SpMV.
- **`D-GPU-02` (Residency)**: All iterates stay device-resident. Zero H2D/D2H memory transfers
  inside the iteration loop.
- **`D-GPU-03` (Sovereign SpMV)**: Custom warp-per-row CSR SpMV kernel. cuSPARSE is used only
  as an external benchmark reference, never linked into the shipped solver.
- **`D-GPU-04` (Adaptive Restarts)**: Restarts on normalized duality gap drop iteration count
  by orders of magnitude (Applegate et al. 2023).
- **`D-GPU-05` (Stepsize & Weight)**: Adaptive stepsize heuristic ($\eta = 0.95 / L_{\text{local}}$)
  with Chambolle–Pock primal/dual weight updates for robust convergence.
- **`D-GPU-06` (Preconditioning)**: Reuses CPU Ruiz equilibration (`src/scale/ruiz_scaling.cpp`)
  and Chambolle–Pock diagonal preconditioning.
- **`D-GPU-07` (Precision)**: FP64 by default for defensible feasibility tolerances ($\le 10^{-6}$).
  FP32 is permitted only as a labelled ablation with FP64 residual verification.
- **`D-GPU-08` (Timing Accounting)**: Mandatory four-part timing emitted in JSON telemetry:
  `h2d_ms`, `kernel_ms`, `d2h_ms`, and `total_ms`. Never report kernel time alone.
- **`D-GPU-09` (Termination)**: Relative KKT error evaluation across primal residual, dual residual,
  and duality gap at separate tolerances ($10^{-4}, 10^{-6}, 10^{-8}$).
- **`D-GPU-10` (Build Flag)**: Optional CMake flag `-DMARKOV_CERO_ENABLE_CUDA=ON`. Without it,
  the project builds and passes all tests without a GPU.
- **`D-GPU-11` (Equivalence)**: CPU/GPU equivalence test is a hard merge gate.
- **`D-GPU-12` (Minimal Interface)**: Solver integration is a single option:
  `struct Options { Backend backend = Backend::cpu; };` in `pdlp.hpp`.

### Module Structure

```text
gpu/
├── include/markov_cero/gpu/
│   ├── device.hpp          # Device query, capability check, graceful fallback
│   ├── buffer.hpp          # DeviceBuffer<T>: RAII, zero-copy, explicit transfers
│   ├── csr.hpp             # DeviceCsr: values, col_idx, row_ptr from SparseCsc
│   └── kernels.hpp         # SpMV, vector axpy, bound projections, reductions
├── src/
│   ├── device.cpp  buffer.cpp  csr.cpp
├── kernels/
│   ├── spmv.cu  vector_ops.cu  reduce.cu  pdhg_step.cu
└── tests/
    ├── equivalence_test.cpp # Kernel-level validation against CPU implementation
    └── pdhg_gpu_test.cpp    # Full solve equivalence and tolerance verification
```

---

## 5. RTX 2050 hardware results and crossover study

A local CUDA 12.9.86 toolchain was installed under `/tmp` for this run. The solver was built
with `MARKOV_CERO_ENABLE_CUDA=ON` for `sm_86`, and the repository benchmark runner executed
CPU PDLP and GPU PDLP on the physical GeForce RTX 2050 (4 GiB, driver 610.57.04). All four GPU
solutions passed the solver's verification gate. Detailed hardware and timing data are in
`evidence/gpu_hardware_rtx2050.json` and
`evidence/benchmarks/crossover_study_gpu_rtx2050.csv`.

| Instance | NNZ | CPU PDLP (ms) | GPU total (ms) | GPU/CPU | GPU verified |
|---|---:|---:|---:|---:|---|
| `scale_100k` | 14,400 | 23.13 | 199.89 | 8.64× | yes |
| `scale_500k` | 67,600 | 238.36 | 741.76 | 3.11× | yes |
| `scale_1m` | 129,600 | 596.25 | 1,976.67 | 3.32× | yes |
| `scale_5m` | 640,000 | 11,980.72 | 31,148.29 | 2.60× | yes |

Simplex was skipped for these cases because each exceeded the configured 1,000-row threshold;
this is a CPU-PDLP versus GPU-PDLP comparison. GPU lost end-to-end on all four measured sizes.
Transfer overhead is substantial (about 87–111 ms H2D on each case), and GPU PDLP also used
more iterations than CPU PDLP on these instances. The source implementation is functional, but
this hardware run does not demonstrate a performance benefit. Further GPU optimization is a
separate decision; do not claim a crossover from these measurements.

The large-QP activation test also passed on this device, but inspection shows the QP GPU path
offloads only the residual `P*x` multiplication. KKT factorization and the ADMM x-update remain
on CPU; therefore the planned GPU x-update has not been implemented yet.

All 11 registered GPU CTests pass on the physical device. The first hardware run exposed a
duplicated `rho` diagonal entry in the QP `P + rho I` builder; the existing-diagonal case is
fixed and the device-vs-CPU ADMM matrix-vector test now passes.

The AXPY kernel was also separately compiled to PTX and run through the CUDA Driver API, with
exact expected output; see `evidence/gpu_hardware_kernel_run.json`.

---

## 6. Profiling evidence status

The older profiling notes in `evidence/benchmarks/nsight_profile_analysis.md` and earlier
revisions of this document are not tied to the RTX 2050 hardware run recorded above. They
include device-residency and kernel-share figures that were not reproduced in this run; do
not cite those values as current hardware measurements. The current evidence records the
four-part timings from the solver runner, but no fresh Nsight occupancy or roofline capture.

## 7. Continuous Integration & Local Verification Policy

In continuous integration (`.github/workflows/ci.yml`), tests execute across standard CPU runners
in graceful CPU fallback mode with full CPU-side unit and integration testing. Hardware GPU
acceleration is verified on local workstations equipped with NVIDIA CUDA GPUs (testing bitwise
equivalence, device residency, and four-part timing).
