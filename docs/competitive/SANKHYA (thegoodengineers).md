---
type: competitive-note
tags: [competitive, t1, sih, sih26119]
status: verified
source: github.com/thegoodengineers/SANKHYA, github.com/Deekshith2205/sih-26
cloned: 2026-09-25
threat: very-high
---

# SANKHYA (thegoodengineers)

> **Rank 1 threat.** The only rival closing R4+R5+R7+R8+R16+R10 simultaneously — and its CI
> fails the build if a solver library is linked, duplicating our provenance differentiator.

**Two public snapshots of one project** [Observed]: `Deekshith2205/sih-26` shares 192 file
paths with `thegoodengineers/SANKHYA` (same `docs/BENCHMARKS.md` generator script, same
`data/casestudies`, `bindings/python`), while SANKHYA holds 673 unique paths — treat Deekshith
as an earlier/side snapshot (32.6k LOC, 6 CI jobs) of the same brand. A *third* unrelated
project also brands itself sankhya: [[sankhya (team-vertexx)]].

## Engines

- **IPM (R4) — real**: `src/ipm/` = 216 KB; `ipm.cpp` 129,708 B, Mehrotra predictor-corrector
  declared `src/ipm/ipm.cpp:2,5-6` with Gondzio multi-centrality correctors `:400-416, :1334`.
- **Crossover**: `src/simplex/crossover.cpp` (18 KB), default **on** (`src/util/options.cpp:1344`).
- **Simplex**: revised primal + dual; OpenMP pragmas `src/simplex/primal_simplex.cpp:728`,
  `dual_simplex.cpp:361`.
- **GPU**: PDHG CUDA (`src/pdhg/pdhg_parallel.cpp:34,50`), honest measurements — 1.37–1.63×
  laptop RTX 5050, 4.21× on L4, 6.47× Mittelmann brazil3; GPU-vs-OR-Tools gate still
  **"Not yet run"** (`docs/BENCHMARKS.md:461`) — our timing window on R8.
- **MILP**: root Gomory GMI cuts **ON by default** (`src/util/options.cpp:993-996`), parallel
  tree (`src/mip/branch_and_bound_parallel.cpp:363-367`, `mip_threads`).
- **Parallel (R7)**: single OpenMP gateway `src/util/threads.hpp:16-27`; CMake `SANKHYA_WITH_OPENMP ON`.

## Evidence & CI (R16/R18)

- `docs/BENCHMARKS.md:149`: **"81 of 89 instances … matched their published optimum to a
  relative 1e-6 and passed independent verification"**; `:1301`: **"We are 2.62x slower than
  HiGHS … publish that rather than bury it."** Generated from committed CSVs (cannot drift).
- `.github/workflows/ci.yml`: `thread-sanitizer` (:29), `fuzz-readers` (:64),
  `build-and-test` matrix Release/Debug+ASan+UBSan (:115), `benchmarks` netlib (:669),
  `netlib-medium` verifier gate (:724), **`provenance` job: `ldd build/sankhya` hard-fails at
  `:814-816` if a solver library is linked**, `reproduce` fresh-machine job (:842),
  `cuda-build` (:975), `format` (:1012), plus `direct-push-guard.yml`.
- HiGHS run as separate process; exact-rational Koch recompute; instance sha256 + commit +
  machine stamp on every CSV.

## Weak spots we can use

- GPU-vs-OR-Tools comparison promised but never run (`BENCHMARKS.md:461`).
- **No LICENSE file** (SPDX `Apache-2.0` at `CMakeLists.txt:1` only).
- 139k LOC including tests — evaluator diffing "student-written vs generated" may notice scale.
- Deekshith snapshot's README claims match reality (e.g. `README.md:242` "CUDA backend is
  unwritten… No speed-up is claimed") — do not accuse this team of overclaiming; they don't.

## R10 check

FetchContent only fmt/CLI11/nlohmann_json/zlib/googletest (`CMakeLists.txt:72-155`); no
HiGHS/COIN/SCIP/OR-Tools anywhere; enforced by the ldd gate. Licence clean.

**Respect:** their evidence discipline is the field's highest. **Beat:** R8 GPU gate still
open; nothing in their repo beats a from-scratch traceability story ([[21-traceability]]).
Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]
- [[refinery-optimizer (kavinR-11)|competitive/refinery-optimizer (kavinR-11)]]
- [[sankhya (team-vertexx)|competitive/sankhya (team-vertexx)]]