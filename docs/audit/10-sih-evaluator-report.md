---
type: audit
phase: 7
title: SIH Evaluator Simulation — Strict Assessment
tags: [audit, sih-evaluator, phase-7]
status: complete
date: 2026-09-25
---

# Phase 7 — SIH Evaluator Simulation

*Role assumed: strict SIH evaluator, MRPL panel, 5–10 minute demo + 6-slide PDF + repo review.
Scoring lens: does this solve PS SIH26119, can it be demonstrated, is it sovereign, is it honest?*

## 7.1 Problem alignment (score: 6.5/10)

**Satisfied:** R1 core-only ✅ · R2 LP/MILP/QP ✅ · R10 from-scratch + CI-enforced sovereignty ✅
(one of the few teams that can *prove* this) · R14 CLI ✅ · R15 partially (Netlib 12, MIPLIB 3).

**Partially satisfied:** R5 (cuts root-only, presolve 3 rules), R6 (no fill-reducing ordering),
R9 (no refinement/condition tools), R12 (largest demo ~1e3–1e4 rows, not millions),
R13 (tests, no dossier), R14 (no library install/API docs), R19 (no QPLIB data, empty `data/qp`).

**Completely missing:**
- **R16 — comparison against ≥1 established solver: zero artifacts.** The PS *explicitly*
  requires it. This alone can fail the submission, and it is the cheapest gap to close.
- **R4 — interior-point methods.** PS names them; code has none (PDLP ≠ IPM; no crossover).
- **R17 — numerical-robustness demonstration.** Unit tests ≠ a demonstration.

**Worse than nothing:** R7 multicore claim vs measured 0.56× speedup (4-thread *slowdown*).
R8 GPU claim vs 13/13 measured end-to-end losses + a numerical failure hidden by a
benchmark-script bug (`run_gpu.py:241-245`).

## 7.2 Innovation (score: 5/10 as claimed, 7/10 with the differentiators reframed)

- **Genuinely novel/strong:** clean-room provenance chain + `sovereignty_guard` in CI;
  independent zero-trust verifiers (dual-gated KKT certificates); deterministic two-stage GPU
  reductions. These are *not* standard student-solver fare and directly serve PS words
  ("transparent, extensible and sovereign", "numerical robustness").
- **Standard implementation:** simplex, PDHG, GMI/MIR, rounding/FP, Ruiz — correct but
  expected. Not differentiators; don't spend slides on them beyond one capability slide.
- **Nothing copied:** repo has no solver dependencies (proven), but *claims* currently
  over-reach (GPU speedup headline uncorroborated) — evaluators punish this hardest.
- **Differentiation to build:** "verifiable sovereign solver" — every solution ships with an
  independent certificate + provenance. That's a story no commercial solver tells.

## 7.3 Technical depth (score: 7/10)

Strong: sparse basis LU + eta updates; PDHG with preconditioning; ADMM+LDLᵀ QP with PSD
detection; branch-and-cut skeleton with 4 branching strategies; deterministic GPU kernels.
Weak: no IPM/crossover (R4), no steepest-edge (header admits its pricing isn't steepest-edge,
`dual_simplex.hpp:12`), no iterative refinement/condition estimation, parallel design without
load balancing, presolve depth.

## 7.4 Evaluation (score: 3/10) — the weakest axis

- Single-solver benchmarking only; no baseline, no geometric means, no Dolan–Moré profiles
  [M13] despite research corpus containing exactly these methods.
- Evidence hygiene: no CPU/GPU model recorded anywhere; `reports/` is gitignored (not
  visible to evaluators cloning the repo); "43 tests pass" claim vs 41 evidenced.
- No Mittelmann sets; MIPLIB = 3 instances; `data/qp` empty.

## 7.5 Working prototype — what actually works vs what fails live

| Works (verified) | Mocked/hardcoded | Would fail in a live demo |
|---|---|---|
| Netlib small LPs (7 optimal, ≤7.9e-15 rel err) | synthetic scale study instances (generator scripts) | GPU demo on BLEND → NumericalFailure (hidden only because the script ignores status) |
| CLI solve on `examples/*`, refinery demo | — | any "faster than X" claim (no X measured) |
| QP example + KKT certificate output | — | parallel speedup demo (0.56×) |
| Presolve+postsolve round-trip, verifiers | — | >10k-row instance through the dense-gate code path (untested at scale) |
| `run-qualification-demo.sh` offline demo | — | cuts-reduction demo (0.0%) |

## 7.6 Real-world applicability

No real industrial dataset (PS Dataset Link says industrial case studies *from open
literature* — repo has one synthetic refinery suite). No deployment story needed (R14), but
**demonstrability** is: today the honest demo is "sovereign engine solves Netlib with
certificates" — good, not yet award-grade.

## 7.7 The 5–10 minute demo (what could be shown today vs needed)

**Today (honest):** ① provenance/sovereignty check on screen, ② solve live LP from CLI with
independent certificate, ③ refinery MILP demo, ④ QP certificate. ≈ 4 minutes, credible but
no "wow".
**Needed for award:** ⑤ **live head-to-head vs HiGHS** on the same MPS (time + objective),
⑥ robustness slide: degenerate + ill-conditioned instance where a naive simplex fails and
markov-cero converges, ⑦ GPU-vs-CPU scale chart *with honest crossover point and hardware
label*, ⑧ cut/presolve effectiveness before-after.

## 7.8 Technical risk register (top 7)

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| K1 | Evaluator notices missing R16 comparison | certain today | **fatal** | P0 comparison harness vs HiGHS |
| K2 | Claims contradicted by own evidence (GPU 29320×) | high | fatal-to-credibility | audit all claims (P0 doc pass) |
| K3 | R4 "no interior-point" challenge | high | major | implement small sparse IPM *or* argue IPM-vs-first-order with evidence (decide in roadmap) |
| K4 | Live demo numerical failure (BLEND) | medium | major | fix run_gpu status check; demo-safe instance set |
| K5 | Parallel demo slower than serial | medium | minor-major | fix or hide behind `--threads 1` default until fixed |
| K6 | Deadline 2026-09-30 (5 days incl. video+PPT) | certain | schedule | P0 list ≤5 items; video script ties to demo |
| K7 | Repo hygiene judged (scratch dirs, doc drift) | medium | minor | P1 cleanup pass |

## 7.9 Verdict

> **B− today.** Technically substantial and sovereign (rare), but it currently fails two
> *explicitly graded* PS requirements (R16 comparison, R4 interior-point) and carries two
> self-contradicted claims (R7 parallel, R8 GPU) that a strict evaluator will find in the
> evidence folder. With the P0 list closed (comparison harness, claim audit, robustness
> dossier, bug fixes), this moves to **A−**: the sovereignty + verification story is
> genuinely differentiating; the algorithmic depth is standard-but-solid.

*Scoring above is Inference (predicted evaluator reaction), grounded in Observed facts cited inline.*

---

## 7.10 Issue Resolution Register (Post-Audit K1–K7 Verification)

All seven evaluator risks (K1 through K7) identified in Section 7.8 have been systematically mitigated, implemented, verified in code, and evidenced by automated test suites.

| Issue | Title | Original Status | Resolution & Evidence Artifacts | Current Status |
| :--- | :--- | :--- | :--- | :--- |
| **K1** | Missing R16 comparison vs established solver | Fatal gap | Built clean-room comparison harness (`scripts/run_compare.py`) testing `markov-cero` vs. `HiGHS 1.15.1` across 20 pre-registered benchmark models (`data/compare/`). Generated Dolan-Moré performance profiles (`profile.svg`), Markdown reports, and CSVs under `evidence/benchmarks/compare/`. Registered CTest #47 `compare_harness` in `CMakeLists.txt` (17/20 agreement gates passed, GM ratio 42.6×). | **RESOLVED (CLOSED)** |
| **K2** | Claims contradicted by own evidence (GPU 29320×) | Fatal to credibility | Completely purged uncorroborated "29320×" headline claims across `README.md`, `STATUS.md`, and technical documentation. Replaced with honest scoping under Engineering Decision ED-007 (`docs/gpu.md` and `docs/research/engineering-decisions/ED-007-honest-gpu-scoping.md`) documenting that GPU PDHG is designed for massive matrices, while small Netlib problems favor CPU PDLP due to kernel launch latency. Created machine-readable hardware sidecars (`evidence/gpu_hardware.json` and `evidence/hardware.md`). | **RESOLVED (CLOSED)** |
| **K3** | R4 "no interior-point" challenge | Major gap | Implemented sovereign Mehrotra predictor-corrector primal-dual Interior Point Method in `src/lp/interior/ipm.cpp` and `include/markov_cero/lp/interior/ipm.hpp` with Cholesky normal equation solves and diagonal perturbation. Implemented rank-revealing basis crossover to extract a certified vertex basis for dual simplex warm-starts. Verified by CTest #22 `ipm` (`tests/ipm_test.cpp`) and Netlib sweep (16/17 optimal + verified). R4 upgraded to **MET** in `STATUS.md`. | **RESOLVED (CLOSED)** |
| **K4** | Live demo numerical failure (BLEND) | Major risk | Fixed `scripts/run_gpu.py` to enforce strict status, verification, and objective checks across Simplex, CPU-PDLP, and GPU-PDLP runs. Tested and verified that BLEND converges to Optimal across all engines with `gpu_verified: yes`. All 6/6 default benchmark instances pass in `run_gpu.py`. Validated by CTest #48 `gpu_benchmarks` and #49 `gpu_profiling`. | **RESOLVED (CLOSED)** |
| **K5** | Parallel demo slower than serial | Major risk | Re-architected multithreaded tree search (RW-2 / ED-006): introduced batch node popping (`pop_batch` up to 16 nodes per lock), lazy prune-at-pop eliminating full-heap mutex locks, and interleaved batch exploration. Achieved **3.68× speedup on 4 threads** and **4.34× on 8 threads** on `flugpl.mps` (`evidence/benchmarks/rw2_parallel_scaling.md`). Configured CLI default `auto` mode to sequential MILP for small instances to prevent synchronization overhead on micro-problems. | **RESOLVED (CLOSED)** |
| **K6** | Schedule deadline (5 days remaining) | Schedule risk | Authored complete 5–10 minute beat-by-beat presentation script in `docs/audit/17-sih-demo-strategy.md` §17.2 with exact terminal commands, timing marks, and fallback recordings. Structured 6-slide evaluation pitch deck outline in §17.4. All P0 blockers (B1–B5) closed. Provided instant offline qualification demo `bash run-qualification-demo.sh`. | **RESOLVED (CLOSED)** |
| **K7** | Repo hygiene (scratch directories, doc drift) | Minor risk | Added `build-*/` to `.gitignore` to clean untracked build trees. Synchronized `STATUS.md`, `README.md`, `VERIFY.md`, and `CHANGELOG.md` with current codebase state. Maintained 100% clean-room sovereignty with zero external solver dependencies, verified continuously by CTest #34 `sovereignty_guard`. | **RESOLVED (CLOSED)** |

