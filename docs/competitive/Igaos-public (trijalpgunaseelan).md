---
type: competitive-note
tags: [competitive, t1, sih26119]
status: verified
source: github.com/trijalpgunaseelan/Igaos-public
cloned: 2026-09-25
threat: high
---

# Igaos-public (trijalpgunaseelan)

> **Rank 4.** Broadest student engine set found (IPM, MIQP, NLP, IIS) with commercial-solver
> CSVs — but no CI (while README claims it) and a squashed single commit with an unfilled
> `<TEAM NAME>` licence.

## Provenance vs [[IGAOS (Lothnic)]]

**Same brand, not a fork** [Observed]: zero byte-identical files; incompatible layout
(Lothnic `src/simplex/simplex.cpp` + pybind vs flat `src/*.cpp` + `include/igaos/*.hpp`);
Lothnic = MIT, this = custom SIH licence with placeholder `Copyright (c) 2026 <TEAM NAME>,
<INSTITUTION>`; no "lothnic" string anywhere; history is one squashed commit `914c498
"PUBLIC REPO"`. Shared: PS name, Haverly textbook case. Treat as an independent sibling —
*or an unpublished team repo dropped as one commit* (squash = the red flag).

## Engines [Observed]

- 58 C/C++ files, 17.8k LOC: revised simplex (`simplex.cpp` 874 L), **Mehrotra IPM + crossover**
  (`ipm.cpp` 692 L, `crossover.cpp`), PDHG (`pdhg.cpp` 285 L + `cuda/pdhg_kernels.cu` —
  README:51 admits CUDA "has **not been executed**… Compiling is not running"), cuts
  GMI/knapsack/MIR (`cuts.cpp` 779 L), branch-and-cut (`solver.cpp` 1227 L), presolve (528 L),
  **MIQP** (`miqp.cpp` 414 L), **NLP/MINLP** (`nlp.cpp` 688 L), IIS/certificates, sensitivity;
  OpenMP (`CMakeLists.txt:125`).

## Evidence (R16 — strong) [Observed]

- `bench/results_commercial.csv` 23 rows with `cplex_obj` + `gurobi_obj`;
  `bench/results.csv` 49 rows with `highs_obj` + rel_diff; `results_netlib.csv` /
  `results_miqp.csv` 115/241 rows.
- README.md:574-575: **"13/13 CPLEX, 21/22 Gurobi"**; failures kept in CSVs (`bell5 … DIFFERS`).
- `bench/commercial.py:62,115` imports `cplex`/`gurobipy` **bench-only**; CMake links only
  OpenMP/libm → R10 clean.

## Weak spots

- **No CI in this snapshot** (no `.github/`) despite README claiming CUDA compiles "in CI" —
  a claims-vs-reality gap we can cite without attacking honesty elsewhere.
- Squashed provenance + unfilled licence placeholder (legal hygiene fail for a sovereign claim).
- No committed hardware/machine metadata found.

**Respect:** breadth + commercial-solver tables nobody else has. **Beat:** verifiability —
one-commit dumps with claimed CI lose to our 43-test CI + provenance trail on inspection.
Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]