---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "gpu/src/pdhg_step.cpp:357"
  - "gpu/include/markov_cero/gpu/pdhg_step.hpp:98"
verified_on: 2026-09-25
---

# GPU-PDHG-Engine

> Device-resident PDHG iteration engine with CUDA kernels and a CPU fallback path.

## Responsibility
- Keep PDHG iterates, preconditioners and SpMV workspace in GPU buffers, run fused iteration chunks with zero per-iteration host transfers, and report H2D/kernel/D2H timing.

## Implementation Facts (Observed)
- Actual files: `gpu/src/pdhg_step.cpp` (driver, 492 lines), `gpu/kernels/pdhg_step.cu`, `spmv.cu`, `reduce.cu`, `vector_ops.cu`; headers under `gpu/include/markov_cero/gpu/`.
- Public entry `solve_pdlp_gpu(model, options)` declared gpu/include/markov_cero/gpu/pdhg_step.hpp:98, defined gpu/src/pdhg_step.cpp:357; reached from src/lp/first_order/pdlp.cpp:174-175 when `--backend gpu`.
- CUDA is compile-time optional: `MARKOV_CERO_ENABLE_CUDA` default OFF (CMakeLists.txt:7), defines `MARKOV_CERO_HAS_CUDA` (CMakeLists.txt:71-72); all .cpp GPU files guard with `#ifdef MARKOV_CERO_HAS_CUDA` (e.g. gpu/src/buffer.cpp:9).
- Without CUDA the device reports `"CPU Fallback (No CUDA)"` (gpu/src/device.cpp:62) and `pdhg_step` dispatches "to CUDA if enabled, else CPU" (gpu/include/markov_cero/gpu/pdhg_step.hpp:64-68).
- `PdhgState` holds device buffers for `x, x_bar, y, x_avg, y_avg, At_y, Ax_bar`, plus `tau/sigma` preconditioners and `eta/omega` scalars (gpu/include/markov_cero/gpu/pdhg_step.hpp:20-51).
- Driver applies Ruiz scaling to a model copy first (gpu/src/pdhg_step.cpp:369-373), uploads A and A^T as CSR (gpu/src/pdhg_step.cpp:391-395), then runs chunks of `restart_every` iterations per timing window (gpu/src/pdhg_step.cpp:434-443).
- Convergence/restart logic mirrors the CPU engine: adaptive restart on residual score and primal-weight update (gpu/src/pdhg_step.cpp:454-486).
- H2D/kernel/D2H millisecond counters are accumulated and returned in `PdlpResult` (gpu/src/pdhg_step.cpp:399-400, 441-442, 415-416) and surfaced as JSON fields (apps/json_output.hpp:178-183).

## Dependencies
- [[PDLP-Engine]], [[RuizScaling]], [[Solution-JSON-Writer]]

## Used By
- [[PDLP-Engine]] (backend dispatch), [[CLI-MarkovCeroSolve]] (`--backend gpu`)

## Research Justification
- [[Primal-Dual Hybrid Gradient]]

## Open Questions / Risks
- Equivalence of CPU and CUDA iteration kernels is covered by gpu/tests/equivalence_test.cpp (existence verified; results UNVERIFIED here).
- Inference: without CUDA the "gpu" backend still runs, but its kernel timings measure host code — numbers may mislead in reports.
