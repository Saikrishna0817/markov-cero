---
type: audit
phase: 0
title: Ground Truth — Four Sources of Truth
tags: [audit, ground-truth, phase-0, sih26119]
status: superseded
date: 2026-09-25
---

# Phase 0 — Establish the Ground Truth (Historical Snapshot)

> **Superseded snapshot:** this document was captured on 2026-09-25 and has not been
> revalidated against the current tree. Its C.3 claims include known stale/contradicted
> statements (for example, the current tree now contains an IPM and the RTX 2050 GPU run is
> recorded in `evidence/gpu_hardware_rtx2050.json`). Treat this file as audit history, not as
> current implementation status; use `STATUS.md` and the fresh 2026-09-28 audit deliverable.

This document separates four sources of truth that the project has been conflating.
Every later audit phase cites these labels:

- **[A] Problem statement** — what SIH/MRPL actually asked for (verbatim: [[sih26119_problem_statement|SIH26119 Problem Statement]]).
- **[B] Research literature** — what the 204 collected references suggest (index: `docs/research_paper_references.md`).
- **[C] Current codebase** — what the repository demonstrably does (evidence: file paths + line numbers).
- **[D] Final solution** — what the team should build (defined in `docs/audit/12-keep-remove-rebuild.md` → `docs/audit/14-target-architecture.md` → `docs/audit/15-roadmap.md`).

Evidence labels used throughout: **Observed fact** (verified by direct inspection),
**Research finding** (from a reference), **Inference** (deduction), **Recommendation** (proposal).

---

## A. What the problem statement actually requires

Source: `docs/sih26119_problem_statement.md` (verbatim portal record, retrieved 2026-09-25).

### A.1 Original problem statement (summary of the verbatim text)

Build a **sovereign mathematical optimization solver core** — not a modeling environment —
from scratch, on mathematical foundations, without building on any existing open-source
solver library. Initial scope LP + MILP + QP; architecture extensible later to
MIQP/NLP/MINLP. Emphasis on numerical stability, scalability and reliable convergence on
large sparse industrial problems (thousands to millions of rows/columns), including
degenerate and ill-conditioned models. Deliverables: a basic API or CLI (no GUI), results
on MIPLIB/Netlib/Mittelmann benchmarks, **a comparison against at least one established
commercial or open-source solver**, and a demonstration of numerical robustness.

### A.2 Required objectives

| # | Objective | PS anchor |
|---|---|---|
| R1 | Solver core, not modeling env | "sovereign mathematical optimization solver core" |
| R2 | LP, MILP, QP initial scope | "LP, MILP and QP as the initial focus" |
| R3 | Modular → MIQP/NLP/MINLP later | "modular architecture that can later be extended" |
| R4 | Revised simplex **and interior-point** | "revised simplex and interior-point methods" |
| R5 | B&B, B&C, cuts, presolve, heuristics, node selection | Description ¶2 |
| R6 | Sparse matrices + numerical linear algebra | Description ¶2 |
| R7 | Multi-core parallelization | Description ¶2 |
| R8 | GPU where *measurable* benefit | "considered where it provides measurable benefits" |
| R9 | Stability/scalability/convergence emphasis | Description ¶2 |
| R10 | **No existing open-source solver library; from scratch** | Description ¶2 |
| R11 | Industrial modeling scope (refinery, blending, planning…) | Description ¶2 |
| R12 | Scale: thousands→millions of variables/constraints | Benchmark sentence |
| R13 | Robustness on degeneracy/ill-conditioning/weak relaxations | Benchmark sentence + Expected Solution |
| R14 | API or CLI; **no GUI required** | Expected Solution ¶1 |
| R15 | MIPLIB/Netlib/Mittelmann benchmark results | Expected Solution ¶1 |
| R16 | **Compare vs ≥1 established solver** | Expected Solution ¶1 |
| R17 | Numerical robustness demonstration on hard instances | Expected Solution ¶1 |
| R18 | Transparent, extensible, sovereign foundation | Expected Solution ¶1 |
| R19 | Datasets: MIPLIB, Netlib, Mittelmann, QPLIB + industrial case studies | Dataset Link field |
| R20 | Optimal/near-optimal on industrial-scale, practical times | Benchmark sentence |

### A.3 Expected inputs

- Optimization models in a standard exchange format. PS names benchmark *libraries*
  (MIPLIB, Netlib, Mittelmann, QPLIB) whose canonical format is **MPS** (and QP data via
  QUADOBJ/QMATRIX/Q section for QPLIB). MPS is *inferred* — the PS never names a format.
- **Observed fact:** repo supports free-format MPS only (`src/io/mps.cpp`). No LP-format,
  no NL, no matrix-market reader.

### A.4 Expected outputs

- Feasible/optimal solutions with objective values; quality + runtime **compared against
  an established solver** (R16). CLI/API output is unspecified by PS (R14).
- **Observed fact:** repo emits JSON solution files (`--output x.json`) and console summaries.

### A.5 Target users

Indian optimization practitioners in refining/petrochemical/power/logistics/manufacturing
and decision-support vendors who currently rent CPLEX/Gurobi/Xpress and cannot inspect or
tailor solver internals (PS Background). Secondary: the SIH evaluator.
**Observed fact:** no user research, no user documentation persona beyond `QUICKSTART.md`.

### A.6 Core constraints

| Constraint | Source |
|---|---|
| C1 Build from scratch; no existing open-source solver library | PS R10 (hard) |
| C2 Sovereignty/transparency: inspectable, modifiable internals | PS Background (hard) |
| C3 No GUI needed — API/CLI sufficient | PS R14 |
| C4 Must run on realistic hardware; GPU only where it measurably pays (R8) | PS R14/R8 |
| C5 Idea-submission deadline 30 September 2026 | portal record (5 days from audit date) |
| C6 SIH process: 6-slide PDF deck, non-AI demo video with live team narration, public GitHub partial implementation | SIH 2026 rules |

**Inference:** C1 is *not* violated by benchmarking against another solver — running an
established solver as an external oracle for comparison is explicitly *required* by R16 and
is not "building upon" it. The repo's sovereignty guard (`scripts/check-sovereignty.py`)
correctly forbids linking/vendor-sourcing; it must not be misread as forbidding comparison.

### A.7 Evaluation criteria (from the PS + SIH format)

1. Correctness on standard benchmarks (R15, R20).
2. A real head-to-head comparison vs ≥1 established solver (R16) — *binary: exists or not*.
3. Demonstrated numerical robustness on degenerate/ill-conditioned/weak-relaxation cases (R17).
4. Sovereign provenance (R10) — checkable from repo history/PROVENANCE.
5. Demonstrability in a short live demo (SIH video/demo rules).
6. Architecture extensibility to MIQP/NLP/MINLP (R3) — judged structurally, not functionally.

### A.8 Expected deployment environment

Not specified by PS beyond "API or command-line interface" (R14).
**Inference:** a developer workstation / CI runner / industrial Linux server. No cloud, no
container, no GPU-mandatory requirement (R8 makes GPU optional/beneficial-only).

### A.9 Expected scale

"thousands to millions of variables and constraints" (R12), large & sparse, with
"highly degenerate models, ill-conditioned matrices and difficult mixed-integer
formulations" (R13).

### A.10 Domain assumptions

Refinery scheduling, crude blending, process optimization, production planning, logistics,
power dispatch, transportation, supply chain (R11); datasets from MIPLIB/Netlib/Mittelmann/
QPLIB plus industrial case studies (R19).

---

## B. What the research literature suggests

- **Observed fact:** 204 references across 17 modules are indexed in
  `docs/research_paper_references.md` (priority ★/✦/○ marks), plus a 714-line historical
  registry `docs/references.md` and an 8-line `docs/consolidated_knowledge.md`.
- **Observed fact (this is the gap):** the references are a *reading list*, not an applied
  knowledge base. There are no per-paper notes, no requirement extraction, no
  research→code mapping anywhere in the repo as of this audit.
- Full extraction is Phases 1–3 of this audit → `docs/research/`.

**Preview of B (research-derived requirements), verified in Phase 6:**
interior-point methods + crossover (R4), degeneracy handling (dual simplex with
Steepest Edge / Harris, KKT-MIX based repairs), MILP beyond root-only cuts, sparse LU
scaling (markowitz / fill-reducing ordering), tuning via Mittelmann-style geometric means,
numerical safeguards (iterative refinement, condition estimation).

---

## C. What the current codebase currently does

All **Observed facts**, verified by direct file inspection during this audit.

### C.1 Project identity

| Item | Value |
|---|---|
| Root | `/home/saikrishna/markov-initial-build` (git, clean except newly added docs) |
| Release | v0.5.2, "Phase 6" complete; Phases 7–8 deferred (README:43) |
| License/provenance | Apache-2.0, `PROVENANCE.md` claims clean-room since 2026-09-13 |
| Language/build | C++20, CMake ≥3.25, presets (gcc/clang × debug/release, gcc-asan-ubsan) |
| CI | single workflow `.github/workflows/ci.yml`, gcc/clang × Debug/Release/ASan-UBSan/TSan |
| Size | src 7,872 LOC · include 1,416 · gpu 4,091 · apps 1,024 · tests 4,936 |
| Tests | ~40 test executables, 43 `add_test` including `sovereignty_guard`, `netlib_benchmarks`, `miplib_benchmarks` |

### C.2 Current architecture (one paragraph)

MPS text → `canonicalize`/`sparse_canonicalize` (CSC model) → optional `presolve` +
`ruiz_scaling` → engine dispatch:
LP: `revised_simplex` (Phase-I/II, Bland) | `dual_simplex` (warm start) | `pdlp` (CPU
PDHG → CUDA GPU offload); MILP: branch-and-cut with GMI/MIR cuts, strong/reliability
branching, rounding/feasibility-pump heuristics, optional `std::jthread` parallel tree;
QP/MIQP: ADMM operator splitting over quasi-definite KKT with Davis LDLᵀ, independent
KKT certificate verifier → postsolve → JSON solution. Full detail:
`docs/audit/07-current-architecture.md`.

### C.3 Implementation status vs Phase claims

| Claim (docs) | Verified state |
|---|---|
| "primal revised simplex certified" | **Observed fact:** implemented with Bland-only anti-cycling; caps 1024×8192, 1e6 iterations (`src/lp/reference/revised_simplex.cpp`) |
| "dual simplex … Harris two-pass ratio test" (README:32) | **Observed fact:** `src/lp/dual/dual_simplex.cpp` has warm-start; Harris/steepest-edge status unverified → tracked as audit item TB-xx |
| "reversible multi-pass presolve" | **Observed fact:** only 4 rules (empty row, row singleton, fixed var, implied-bound-lite) in `src/presolve/presolve.cpp` |
| "PDLP … preconditioning" | **Observed fact:** Ruiz diagonal prescaling + Chambolle-Pock PDHG (`src/lp/first_order/pdlp.cpp`), GPU path via `gpu::solve_pdlp_gpu` |
| "interior-point" | **NOT implemented.** R4 partially met: PDLP is a first-order method, not an IPM; no crossover/basis extraction exists |
| "GPU … verified scale crossover study" | **Observed fact:** `evidence/benchmarks/crossover_study.csv` (13 rows): end-to-end speedup < 1 in **13/13** rows, kernel speedup < 1 in 12/13 — GPU loses to CPU PDLP at every measured scale; the "up to 29320×" headline derives from one configuration and is not corroborated by this file |
| "independently verified" | **Observed fact:** `reference_lp_verifier`, `primal_verifier`, zero-trust KKT verifier exist and are tested |
| "Phase 6 complete" | **Observed fact:** QP/MIQP ADMM + LDLᵀ present |
| Comparison vs established solver | **Observed fact:** NONE anywhere in `evidence/`, `reports/`, `benchmarks/` → **PS R16 unmet** |
| Numerical robustness demonstration | **Observed fact:** degeneracy/ill-conditioning test *suites* exist (tests), but no curated hard-instance demonstration report |
| Multicore parallelization | **Observed fact:** `evidence/benchmarks/phase4.json` shows 4-thread speedup **0.56×** (i.e., slowdown), parallel efficiency 14% → R7 effectively unmet |
| GPU acceleration with measurable benefit | **Observed fact:** `phase4.json` cut-node reduction **0.0%**; `BLEND → NumericalFailure` in `_deployment-phase2-build/gpu_benchmark.csv:3` — root cause: `scripts/run_gpu.py:241-245` never checks `res_simplex["status"]`, so the `gpu_benchmarks` CTest passes anyway (`evidence/local-verification-report.txt:232`); R8 not yet demonstrated |
| Benchmarks: Netlib | **Observed fact:** `evidence/netlib_results.csv` — 7 instances optimal, rel err ≤7.9e-15, 0.53–39.7 ms; `netlib_extended.csv` — 5 more at 1.3–39.7 s; `miplib_results.csv` — only 3 instances |
| Mittelmann | **Observed fact:** no Mittelmann instance sets found → R15 partial |

### C.4 Dead / suspect / duplicate inventory (verified)

- `src/milp/cuts.cpp` deleted but stale `.o` in build tree; `tests/fuzz/mps_coverage_fuzz.cpp`
  unused by CMake; `benchmarks/runners/*.py` byte-duplicates of `scripts/*.py`.
- Scratch/deploy dirs at repo root: `_deployment-phase2-build`, `_m5-deploy-{gxx,clangxx,gxx-sanitize,clangxx-sanitize,fuzz}`, `_verify-{gcc,clang}`, `build/` — all build artifacts.
  **Correction (verified):** they *are* gitignored (`.gitignore:2-6`); they pollute the working
  tree only, not the repo history. Also verified: `benchmarks/runners/*.py` are md5-identical to
  `scripts/*.py`; zero TODO/FIXME comments repo-wide; 43 `add_test` all declared in root
  `CMakeLists.txt:166-221`; CI has 8 jobs (gcc/clang × Debug/Release/ASan-UBSan/TSan) and **no
  CUDA job**.
- `docs/history.md` stops at Phase 4; `VERIFY.md` references non-existent `provenance/`;
  CHANGELOG has holes (0.3.0–0.5.0).
- **No verbatim PS existed in-repo** (PS-GAP-01) — only paraphrases at `README.md:8`,
  `NOTICE:1`, `QUICKSTART.md:19,68`, `docs/references.md:3,58,626`.

### C.5 Existing project goals (as stated in repo docs)

Goal chain: "Phase 1…6" plan ending in "Multi-Engine LP/MILP/QP/MIQP Release" with planned
Phase 7 ML-assisted branching and Phase 8 steepest-edge (README:43), plus a "judge demo"
`run-qualification-demo.sh`. **Inference:** the repo's phase plan tracks *implementation
progress*, not *PS requirement coverage* — nothing in the repo maps phases to R1–R20.

---

## D. What the final solution should actually do

(Deliverable of Phases 8–10; pointer only at this stage.)

1. Satisfy every hard PS constraint: from-scratch sovereign core (C1), API/CLI (R14),
   LP/MILP/QP (R2), extensible modular design (R3).
2. Close the three *binary* evaluator-facing gaps first: **R16 external-solver comparison**,
   **R4 interior-point/crossover** (or a documented, evidence-backed redefinition),
   **R17 robustness demonstration dossier**.
3. Make GPU honest: either demonstrate measurable benefit (R8) per-scale with CPU/GPU
   hardware recorded, or reduce GPU to an evidence-backed niche claim.
4. Repair negative-value work: parallel B&B 0.56×, root-only cuts 0.0%, `BLEND` numerical failure.
5. Package for SIH: 6-slide PDF deck, narrated live demo video, public repo.

**Inference (to be tested by Phase 7):** the codebase is ~70% of a *core*, but its
evaluation story — the part SIH explicitly grades — is <20% complete.

---

## A vs B vs C vs D — first-pass divergence table

| PS requirement | [B] research says | [C] code has | Gap class |
|---|---|---|---|
| R4 interior-point | IPM+crossover is standard (Mittelmann; Bixby; Lustig et al.) | none | **GAP (hard)** |
| R16 vs established solver | Mittelmann methodology: geometric means, matched hardware | none | **GAP (hard)** |
| R7 multicore | parallel B&B needs work-stealing + subtree sync | 0.56× speedup | **REGRESSION** |
| R8 GPU benefit | GPU wins only at crossover scale; needs per-scale evidence | mixed CSV, 1 numerical failure | **UNPROVEN** |
| R5 cuts/heuristics | GMI+MIR good baseline; needs cover/clique/odds, dive/RINS | root-only cuts, 0.0% node reduction | **PARTIAL** |
| R9 robustness | steepest-edge, Harris, KKT-MIX, iter. refinement, equilibration | Bland-only, 4 presolve rules | **PARTIAL** |
| R10 from scratch | clean-room fine; provenance must be demonstrable | `PROVENANCE.md` + sovereignty guard | **GOOD** |
| R15 benchmarks | Netlib small; Mittelmann/MIPLIB are the real test | 7+5 Netlib, 3 MIPLIB | **PARTIAL** |
| R2 QP scope | QPLIB + projected-gradient/ADMM tradeoffs | ADMM+LDLᵀ | **GOOD (verify)** |

---

## Phase 0 gaps (actioned later in this audit)

| ID | Gap |
|---|---|
| PS-GAP-01 | No verbatim problem statement in repo → **fixed** by `docs/sih26119_problem_statement.md` |
| PS-GAP-02 | No requirement-to-phase traceability (R1–R20 → code) → `docs/audit/21-traceability.md` |
| PS-GAP-03 | No external-solver comparison artifacts (R16) |
| PS-GAP-04 | No Mittelmann benchmark set usage (R15) |
| PS-GAP-05 | No recorded CPU/GPU model in any evidence → results not reproducible |
| PS-GAP-06 | No interior-point engine (R4) |
