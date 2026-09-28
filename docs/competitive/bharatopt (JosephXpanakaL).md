---
type: competitive-note
tags: [competitive, t2, sih26119]
status: verified
source: github.com/JosephXpanakaL/bharatopt
cloned: 2026-09-25
threat: medium
---

# bharatopt (JosephXpanakaL)

> T2. Genuinely broad prototype incl. CUDA — but zero recorded benchmark evidence, a
> mislabelled "simplex", and a broken CI YAML that masks test failures.

## Engines [Observed]

- Own code = **5.2k LOC** (18 C/C++ files; the headline 31k includes 24.8k vendored
  `include/nlohmann/json.hpp`).
- Projected PDHG/BB-style LP (`src/solver.cpp:169`), small dense **Mehrotra IPM**
  (`src/interior_point.cpp`, 165 lines), LP-relaxation B&B (`src/solver.cpp:204,249`),
  convex QP (`src/qp.cpp`), spatial B&B + McCormick + SLP nonlinear pooling (`src/pooling.cpp`
  1275 L — refinery-pooling domain fit, notable vs our R11), real CUDA PDHG
  (`src/cuda_backend.cu:3-4`, cuBLAS/cuSPARSE, `__global__` kernels).
- **"Simplex" is a facade**: `src/simplex.cpp:10-13` just sets
  `backend="Simplex-DirectedRounding"` and re-runs `core.solve_lp`.

## Evidence (R16 = claim-only) [Observed]

- `benchmarks/` and `data/` contain README placeholders only — **zero CSVs**.
- `scripts/compare_highs.py` (40 lines) exists but no committed output; README.md:116 puts
  "Netlib/MIPLIB benchmark harness and external-baseline runner" at **roadmap item 8**.
- 1-commit history.

## CI [Observed]

`.github/workflows/ci.yml` contains **two concatenated workflow documents** (`name:` at lines
1 and 33, no `---` separator — YAML invalid as a whole) and one job runs
`ctest … || true` (**failures masked**); other jobs: cpu smoke, docker CUDA compile, ASan/UBSan.

## R10 check

No solver linked (nlohmann only); `DEPLOY.md:113` "HiGHS is a benchmark-only dependency".
Licence: MIT (`LICENSE` present).

**Respect:** pooling-problem domain modelling + real CUDA kernels. **Beat:** they record
nothing — any committed evidence we produce beats their whole bench story by default.
Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]