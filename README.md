# markov-cero

[![CI][ci-badge]][ci-link]

[ci-badge]: https://github.com/Saikrishna0817/markov-zip1/actions/workflows/ci.yml/badge.svg
[ci-link]: https://github.com/Saikrishna0817/markov-zip1/actions/workflows/ci.yml

Clean-room C++20 solver core for SIH 2026 problem SIH26119 (MRPL indigenous LP/MILP/QP).

Current release: **v0.5.2**. The repository contains LP, MILP, convex QP/MIQP,
experimental NLP/MINLP, and CUDA paths. Verification and benchmark coverage vary by
engine; [STATUS.md](STATUS.md) records the measured limits and open acceptance gates.

Substantial implementation was AI-assisted under human direction; see
[provenance and verification](docs/governance/provenance-and-verification.md).
The historical 29,320× GPU speedup comparison is quarantined because its
simplex baseline was unreliable. Current engine-matched GPU results show no
end-to-end speed benefit on the measured cases.

## Scope & Capabilities

- **Model & Storage**: Immutable model, free-format MPS parser, Compressed Sparse Column (CSC).
- **Presolve & Scaling**: Reversible multi-pass presolve with LIFO postsolve stack; Ruiz scaling.
- **Continuous LP Engines**:
  - Certified Primal Revised Simplex with Bland's rule anti-cycling.
  - Dual Revised Simplex with Harris two-pass ratio test, basis serialization, and warm-starts.
  - Sparse basis substrate with row-map Gaussian LU and product-form Eta updates.
  - Matrix-free First-Order PDLP (Chambolle-Pock) with diagonal preconditioning.
  - Interior-Point Method (`--engine ipm`): Mehrotra predictor-corrector primal-dual IPM on the canonical standard form, with a rank-revealing crossover that converts the interior optimum into a certified vertex basis for the dual simplex (so IPM results are warm-startable, per R4/R5). Netlib sweep: 16/17 optimal+verified, of which 8 crossover-certified and 8 via the documented simplex fallback (`tests/ipm_test.cpp`).
- **GPU-Accelerated LP Engine**:
  - Sovereign CUDA First-Order PDLP with device-resident loop (`--engine pdlp --backend gpu`).
  - Warp-per-row CSR SpMV and transpose-SpMV kernels with shuffle reduction.
  - Deterministic two-stage parallel reductions and adaptive restart strategy.
  - Four-part timing telemetry (H2D, kernel, D2H, total) and a physical RTX 2050 crossover run: GPU PDLP was 2.60–8.64× slower than CPU PDLP on all four measured scales; no GPU benefit is claimed (`docs/gpu.md`, `evidence/benchmarks/crossover_study_gpu_rtx2050.csv`). Hardware details for this run are in `evidence/gpu_hardware_rtx2050.json`.
- **MILP Branch-and-Cut Engines**:
  - Sovereign Sequential Branch-and-Cut (`--engine milp`).
  - Multithreaded Parallel Tree Search (`--engine parallel --threads N`) with C++20 `std::jthread` — batched work distribution and lazy pruning (RW-2): 3.68× at 4 threads on flugpl, TSan-clean (`evidence/benchmarks/rw2_parallel_scaling.md`).
  - Cutting Planes: Gomory Mixed-Integer (GMI) cuts and Mixed-Integer Rounding (MIR) cuts.
  - Variable Selection: Strong Branching domain reduction and Reliability Pseudo-Costs.
  - Dual-tier Primal Heuristics: Simple Rounding and Feasibility Pump with cycle perturbation.
- **Convex QP & MIQP Engines**:
  - OSQP operator-splitting ADMM over symmetric quasi-definite KKT (`--engine qp`).
  - Timothy Davis sparse LDLᵀ factorization with exact symbolic fill-in.
  - Positive semi-definiteness detection via LDLᵀ diagonal pivot validation.
  - MIQP branch-and-cut optimization with quadratic objective heuristics (`--engine miqp`).
  - Independent zero-trust KKT certificate verifier (residuals, dual stationarity, gap).
- **Zero-Trust Independent Verification**: Dual-gated verification in canonical and original space.
- **Applications**: `markov-cero-info`, `markov-cero-mps-inspect`, and `markov-cero-solve`.

- **Pricing**: Forrest-Goldfarb exact $O(m)$ Dual Steepest-Edge (DSE) pricing in Dual Simplex.
ML-assisted branching is experimental and explicitly opt-in via `--branching ml_gnn`;
its trained-model and genuine ML-on node-count acceptance gate remains open.

## Solve a model

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
./build/markov-cero-solve examples/blend.mps
./build/markov-cero-solve examples/qp_portfolio.mps --engine qp
./build/markov-cero-solve examples/refinery/refinery-feasible.mps --output /tmp/result.json
```

Judge demo (offline):

```sh
bash run-qualification-demo.sh
```

## Verify

```sh
./scripts/verify-release.sh
```

See `STATUS.md` and `QUICKSTART.md`.
