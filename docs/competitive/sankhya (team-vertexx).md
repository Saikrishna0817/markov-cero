---
type: competitive-note
tags: [competitive, t1, sih26119]
status: verified
source: github.com/team-vertexx/sankhya
cloned: 2026-09-25
threat: high
---

# sankhya (team-vertexx)

> **Rank 2 threat.** R16 comparison runs **inside CI** — every push verifies each Netlib
> instance against its published optimum — and the repo publishes its own retractions.

*Not related to [[SANKHYA (thegoodengineers)]] despite the shared brand; separate team,
separate codebase (Apache-2.0 LICENSE file present).*

## Engines

- Revised **primal and dual** simplex (Devex, steepest edge) `src/sankhya/simplex.cpp`;
  reader matches HiGHS on 88 Netlib, **simplex 82/88 published optima with 0 wrong answers**
  [Observed: `docs/RESULTS.md`].
- PDHG/PDLP with restarts + Halpern + feasibility polishing (`src/sankhya/pdhg.cpp`); LU/LDL.
- Branch-and-cut: cover, c-MIR, Gomory; reliability branching; pump/diving/RINS. MILP
  **45/70 feasible on MIPLIB @15 s**; QP 35/40 Maros–Mészáros.
- **No IPM**: no `ipm*`/Mehrotra/barrier hits; `src/sankhya/crossover.hpp:11-24` explicitly
  says it is *not* "a full crossover in the sense of a barrier code" (PDHG→basis polish).
- Parallel: own thread pool (`src/sankhya/threading.hpp:70`, `threading.cpp:54,134`).
- GPU: CUDA backend `cuda_backend.cu`, Tesla T4 measured 3.14–12.07×.

## Evidence & CI (R16/R18)

- `.github/workflows/ci.yml:31` job `correctness` (needs `unit`): fetches Netlib
  (`scripts/fetch_netlib.py`), then `:52/:58` runs `bench/verify_simplex.py 120` —
  **"Every instance against its published optimum"** — and `:59` "Presolve must not change an
  answer".
- Self-grade honesty: `README.md:60-61` **"about 62% of HiGHS, with MILP still the weak leg
  at roughly 40"**; `docs/RESULTS.md:35` weighted 62/100.
- **Published retractions** [Observed]: `RESULTS.md:221-229` "Correcting an earlier measurement
  of my own … **The 37× should not be quoted**"; `:1422-1430` "That was wrong twice over";
  `README.md:65` "replaced a '16 of 16'". Reads as trustworthy, not marketing.

## Weak spots

- No IPM (R4 gap) — same as us; whoever ships IPM first takes that rubric line against them.
- LP-only scoring story: MILP ~40% of HiGHS is their own admitted weak leg.
- Single-maintainer history tone in RESULTS.md ("my own") — small team surface.

## R10 check

CMake links only `find_package(Threads REQUIRED)` (`CMakeLists.txt:53`); HiGHS invoked as an
external binary in `bench/milp_vs_highs.py:16,37`, never linked. Licence: Apache-2.0.

**Respect:** verifiable-in-CI honesty. **Beat:** IPM lane + independent-verifier depth.
Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]
- [[SANKHYA (thegoodengineers)|competitive/SANKHYA (thegoodengineers)]]