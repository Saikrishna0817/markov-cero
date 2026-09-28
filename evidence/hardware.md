# Hardware manifest for the RTX 2050 GPU validation

This manifest applies to the new CUDA build and measurements dated 2026-09-27 UTC. It does
not retroactively identify the machine or build used for older benchmark files. See each
artifact's own metadata before comparing numbers.

| Item | Value |
| --- | --- |
| OS | Omarchy (Arch-based), kernel 7.2.5-3-omarchy, x86_64 |
| CPU | Intel 12th Gen Core i5-12450HX, 12 logical threads |
| RAM | 11.4 GiB |
| GPU | NVIDIA GeForce RTX 2050, 4 GiB, compute capability 8.6 |
| Driver | 610.57.04 |
| CUDA compiler | NVIDIA CUDA 12.9.86 in `/tmp/markov-cuda-toolchain` (local install; system packages unchanged) |
| C++ compiler | GCC 16.2.1; CUDA host compiler GCC 13.4.0 |
| Solver build | Release, C++20, `MARKOV_CERO_ENABLE_CUDA=ON`, `MARKOV_CERO_ENABLE_ML=ON`; measured binary compiled for `sm_86` |
| Architecture coverage build | Separate `all-major` CMake build succeeded; physical execution was tested on `sm_86` only |

## Results and limits

- The repository GPU CTest set passed **11/11** on the physical RTX 2050, including PDLP kernel
  equivalence, the ADMM matrix-vector regression, and the large-QP activation test.
- GPU PDLP solved and verified all four scale-study cases. GPU end-to-end time was 2.60–8.64×
  CPU PDLP time in these single-run measurements. Simplex was skipped for all four because the
  row counts exceeded the runner's 1,000-row cutoff. The exact records and timing breakdowns
  are in `evidence/benchmarks/crossover_study_gpu_rtx2050.csv`.
- A 110,241-entry convex QP activated the GPU-assisted residual product path, passed independent
  KKT verification, and agreed with the CPU objective. Only residual `P*x` is offloaded; the
  KKT factorization and ADMM x-update remain on CPU.
- CUDA PDLP and QP correctness have been exercised on this RTX 2050. Performance claims are
  limited to the four single-run LP comparisons above; no repeated-trial statistics, other GPU
  models, or GPU speedup have been established.

## Artifacts

| Artifact | Scope |
| --- | --- |
| `evidence/gpu_hardware_rtx2050.json` | Hardware and benchmark-run sidecar |
| `evidence/benchmarks/crossover_study_gpu_rtx2050.csv` | Full CPU/GPU PDLP timing, status, verification, and hardware id |
| `evidence/benchmarks/crossover_study.csv` | Plot input derived from the four same-run CPU/GPU records |
| `evidence/benchmarks/crossover_plot.svg` | Visualization of the current four-case crossover data |
| `evidence/gpu_hardware_kernel_run.json` | AXPY kernel run plus GPU test/build verification summary |
