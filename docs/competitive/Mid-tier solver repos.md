---
type: competitive-note
tags: [competitive, t2, sih26119]
status: verified
cloned: 2026-09-25
threat: low
---

# Mid-tier solver repos

> T2 leftovers: real code but no credible evidence, no CI, or both. None threatens
> markov-cero; all are useful as *field texture* (what a typical SIH submission looks like).

## mohitsaitummalapalli-tech/SIH26119-Indigenous-GPU-Accelerated-Optimization-Solver

C++, 25 files / 11.4k LOC — **infrastructure without algorithms**: row-partial-pivoting LU
(`src/solver/lp/basis_factorization.cpp`, 663 L), standard-form conversion (647 L), MPS/LP
readers (602 L), model (555 L), and a 1,315-line factorization test suite (31 `TEST-FACT-xx PASS`
rows in `docs/PHASE_3C_BASIS_FACTORIZATION.md`). `src/algorithms/`, `src/api/`,
`src/backend/cuda|cpu/`, `src/verification/` = **README.md stubs only**. Honest: README.md:17
"**NO** optimization algorithms … are implemented yet". No presolve, no simplex, no GPU, no
results, no R16. R10 clean (states "Strict Zero-Solver Constraint"). **Verdict: partial,
threat ~medium-low if they ship simplex in 5 days** [Observed].

## Mage-100/nomos

C++ header-only, 6 .cpp + 9 hpp, 1.8k LOC: two-phase simplex with artificial vars + ratio
test + refactorization in `src/LPSolver.hpp` (627 L), basis factorization engine (167 L),
MPS parser (398 L). `grep -iE "gomory|branch|milp|integer|quadratic|presolve|openmp|cuda|highs"`
over `src/` → **0 hits**. Tests commented out in `CMakeLists.txt:12-14`; README = verbatim PS
paste + vcpkg steps. R10 clean (eigen3 + ftxui only). **Verdict: minimal LP prototype, low** [Observed].

## ApexCUDA pair — AashnaDas/ApexCUDA_Airline + ayushmishra2992/ApexCUDA-Optimization

Same team (INDIOPTIMA); **byte-identical README** (4,883 B, `cmp` IDENTICAL) across both repos.
- **The headline "35.79× GPU speedup" is one SpMV**: notebook cell computes
  `cpu_avg_ms/gpu_avg_ms` on a single CSR matvec (43.51 ms scipy vs 1.216 ms cuPy, Tesla T4);
  cell 6–7 shows it decays to 18.43× at 500 iters with 860 ms transfer, break-even ≈21 ops.
  The 3.4M×472k MILP is **never solved**; its only LP solve is `linprog(method="highs")` on a
  **2×2 toy**; `ADMM_SOLVER_1000.ipynb` reports "Maximum constraint violation: 120.0" for x=0.
- **Fake telemetry**: `backend/server.py:70` `time.sleep(0.078)` then literal success strings
  (":89 Optimal solution reached…", ":91 Objective score 18.742041 … in 0.078s");
  `frontend/components/BenchmarkArena.tsx:35,57,72` hardcodes "SCIPY HIGHS · 8,420 ms" vs
  "78 ms" → "🔥 107.9x FASTER" in static JSX.
- The other repo is the real code half: `backend/core/solver.py` (747 L ADMMSolver),
  `milp_solver.py` (259 L B&B, `max_nodes=100`), `formats/mps_parser.py` (527 L), cuPy
  `backends/cuda_backend.py`, 12 tests ("20 PASSED, 1 DEFECT DISCOVERED"). No simplex/IPM;
  `benchmarks/highs_references.json` stores HiGHS values; no committed run outputs.
**Verdict: pitch-only front + partial ADMM engine; threat low — but *the benchmark theatre is
exactly what P0-2 must make sure we never look like*** [Observed].

## hemasri-152006/BharatOpt

Python, 9 files / 1.7k LOC: genuinely dense `TwoPhaseSimplex` (1,115 L, EPS=1e-9, min-ratio
test) + `verifier.py` (independent recompute) + `sif_parser.py`. **No B&B anywhere** despite
MILP framing; repo structurally broken — `benchmark.py:2` imports `solver.sif_parser` and
`main.py` imports `ui.dashboard`, but no `solver/`, `ui/`, or `data/` dirs exist. Benchmark =
Netlib *published optima* only (`benchmark.py:10-11`), no solver-vs-solver. R10 clean.
**Verdict: partial, threat low** [Observed].

## infinity390/smart_india_hackathon_2026_119

3 files, 442 LOC pure Python: `algorithm_A.py` brute-force vertex enumeration over constraint
subsets with `fractions.Fraction`; `algorithm_B.py` vertex-neighbor walking (RREF rank/
nullspace + ray tracing). Exact toy LPs only; no GPU, no tests, no MPS ingestion, no R16.
**Verdict: toy, threat none** [Observed].

## Cross-cutting

None of these seven vendors or links an external solver (no `highspy`/`pulp`/`ortools`/
`gurobipy` in solver code; ApexCUDA uses scipy `linprog` only as a 2×2 reference) → **R10
clean across the tier**. R16 evidence: only HiGHS reference-value JSON (ApexCUDA) and Netlib
published optima (hemasri) — **no reproducible solver-vs-solver table anywhere in this tier**.

Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]