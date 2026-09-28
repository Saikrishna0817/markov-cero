---
type: competitive-note
tags: [competitive, t2, sih26119]
status: verified
source: github.com/Kouhsik33/Bharat-Opt
cloned: 2026-09-25
threat: low
---

# Bharat-Opt (Kouhsik33)

> T2/Low. Single-engine (PDHG CPU+CUDA) repo whose README is refreshingly explicit that
> **nothing has been run yet**.

## Engines [Observed]

- 71 C/C++/CUDA files, 8.5k LOC: modules `core/{pdhg,presolve,parser,sparse,numeric,router,
  fingerprint}`, `gpu/` (`batch_pdhg.cu` 520 L, `kernels.cu` 361 L, `memory.cu` 334 L),
  `verifier/`, `audit/`, `cli/`, `dashboard/`.
- **Restarted PDHG only** — README.md:19-20: "MILP, QP, and higher accuracy are **roadmap
  only**". No B&B, no IPM, no simplex.

## Evidence — none, by their own admission [Observed]

- `benchmark/{netlib,harness}/` hold only `.gitkeep` + scripts.
- README.md:25-26: **"None of it has been built, run, or verified by a human yet."**
- Planned oracle: `system_design.md:174` SciPy `linprog`/HiGHS standalone; `rules.md:43-45`
  forbids any solver as engine (R10-conscious scaffolding).

## Weak spots

No CI (no `.github/`), no LICENSE, single-threaded CPU path, zero measurements.

**Note for us:** this repo (with several others) reads as **AI-scaffolded** — clean structure,
no execution. If the evaluation wave includes such repos, *having run numbers at all* separates
us. Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]