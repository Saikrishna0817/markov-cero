---
type: audit
phase: 21
title: Research → Code Traceability Matrix
tags: [audit, traceability, phase-21, sih26119]
status: complete
date: 2026-09-25
---

# Phase 21 — Research → Code Traceability Matrix (R1–R20)

> **Historical snapshot, not current acceptance status.** This matrix was written against an
> earlier repository state and contains superseded GAP/PARTIAL claims as well as stale
> recommendations (including the former decision to defer ML). For current implementation
> evidence, acceptance gates, and unresolved findings, see
> [`docs/audit/sih_2026_implementation_audit.md`](sih_2026_implementation_audit.md).
> Do not use the statuses below as current claims until each row is re-audited.

Question answered here: *for every PS requirement, what does the research say, what does the
code do, what is the delta, and what artifact proves closure.* Status values are copied from
[[09-research-code-alignment]] §6.1; priorities (P0/P1/P2) are copied from [[12-keep-remove-rebuild]]
and the step order in [[13-restart-point]]. Closes PS-GAP-02 (see [[00-ground-truth]]).

**Status legend:** GOOD = present & evidenced · PARTIAL · GAP · REGRESSION (worse than baseline)
· UNPROVEN (claim exists, evidence absent or contradicting). **Priority legend:** P0 = must close
before 2026-09-30 · P1 = grade/quality work · P2 = polish. **Verification:** no `docs/audit/16-*`
exists yet, so each row names a concrete test or artifact instead.

## 21.1 Requirement matrix

| R# | PS clause (short quote) | Research evidence | Current code | Status | Gap description | Recommendation | Verification |
|---|---|---|---|---|---|---|---|
| **Group A — Scope & foundations (R1–R4)** ||||||||
| R1 | "solver core rather than a complete modeling environment" | [[Achterberg-2007-Constraint-Integer-Programming]] (blueprint for a solver core) · [[Bixby-2002-Evolution-of-LP]] (what a core is made of) · [[Multi-Engine Solver Architecture]] · [[Kumar-2010-Fifty-Years-Integer]] | [[RevisedSimplexEngine]] `src/lp/reference/revised_simplex.cpp:383` · [[DualSimplexEngine]] `src/lp/dual/dual_simplex.cpp:363` · [[QP-ADMM-Engine]] `src/qp/admm_solver.cpp:84` · [[MPSParser]] `src/io/mps.cpp:96` | GOOD | Core exists; no modeling layer was built. Orchestration still lives in the CLI (AP-6), so the *library* half of "core" is thin. | P2 — keep; fold API exposure into RW-4 (see R14) | `ctest -R sovereignty_guard` green; [[07-current-architecture]] pipeline matches `apps/markov_cero_solve.cpp` |
| R2 | "support Linear Programming (LP), Mixed-Integer Linear Programming (MILP) and Quadratic Programming (QP)" | [[Achterberg-2005-General-Mixed-Integer]] · [[Cornuejols-2001-Branch-and-Cut-Algorithms]] · [[Branch and Cut]] · [[ADMM]] · [[Kronqvist-2025-50-Years-Mixed-Integer]] | [[BranchAndCut]] `src/milp/milp_solver.cpp:21` · [[PDLP-Engine]] `src/lp/first_order/pdlp.cpp:173` · [[QP-ADMM-Engine]] + [[LDL-Factorization]] `src/qp/kkt.cpp:13` | GOOD | Three engine families present; QP has no real instance data (`data/qp` empty). | P2 — keep; populate QP data under R19 | `ctest` qp/milp/pdlp targets; `data/qp` non-empty with ≥1 QPLIB-style instance |
| R3 | "modular architecture that can later be extended to MIQP, NLP and MINLP" | [[Multi-Engine Solver Architecture]] · [[CSC Sparse Model]] · [[Presolve-Postsolve Stack]] · [[Achterberg-2007-Constraint-Integer-Programming]] | `--engine auto` dispatch in `apps/markov_cero_solve.cpp` · [[Canonicalizer]] `src/transform/sparse_canonicalize.cpp:119` | PARTIAL | Engine/model seams exist, but extension points are undocumented and unexercised; NLP/MINLP absent (allowed *later*). | P2 — document extension points (R3 judged structurally); API work is RW-4 (P1) | Short extension-points section in `docs/audit/14-target-architecture.md` + a stub engine note; no functional test required |
| R4 | "Core algorithms may include revised simplex **and interior-point methods**" | [[Karmarkar-1984-New-Polynomial-Time-Algorithm]] · [[Mehrotra-1992-Implementation-Primal-Dual-Interior]] · [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]] · [[Wright-1997-Primal-Dual-IPM]] · [[Ye-1998-Crossover-Interior-Point]] · [[No Interior-Point Engine]] · [[No Crossover]] | Simplex half present ([[RevisedSimplexEngine]], [[DualSimplexEngine]]); **IPM: 0 hits** in `src/include/apps`; **crossover: 0 hits** (only `scripts/plot_crossover.py`, unrelated); [[PDLP-Engine]] is first-order, not IPM | GAP | Half of an explicitly named algorithm pair is missing; PDHG cannot be relabelled IPM (different class, see [[First-Order Accuracy Ceiling]]). | **P0** — ED-003: minimal sparse Mehrotra-style IPM + crossover to a basis, *or* a formal evidence-backed re-scope; decide day 1 (step 6, [[13-restart-point]]) | New `tests/ipm_test` + `evidence/ipm_crossover.md`: LP solved, basis certified by [[IndependentVerifiers]], warm-started into [[DualSimplexEngine]] |
| **Group B — MILP & numerics (R5–R9)** ||||||||
| R5 | "branch-and-bound, branch-and-cut, cutting planes, presolve, heuristics and advanced node selection strategies" | [[Cornuejols-2001-Branch-and-Cut-Algorithms]] · [[Achterberg-2005-Branching-Rules-Revisited]] · [[Savelsbergh-1994-Preprocessing-Probing-Techniques]] · [[Fischetti-2006-Feasibility-Pump-Heuristic]] · [[Root-Only Cuts]] · [[Presolve]] | [[BranchAndCut]] root-only cuts `src/milp/milp_solver.cpp:165-208` · [[CutGenerators]] `src/milp/gomory.cpp:12`, `src/milp/mir.cpp:21` · [[Presolve]] 4 rule types `src/presolve/presolve.cpp:20` · [[PrimalHeuristics]] `src/milp/heuristics.cpp:81` | PARTIAL | Skeleton complete (best-bound heap, strong branching, rounding+FP) but cuts stop at the root → 0.0% node reduction; presolve is 4 rules; `Options::node_strategy` never read; no DFS/dive. | **P0** RW-1 in-tree cut loop + pool (ED-005) · P1 RW-6 presolve depth (ED-010) · P2 diving | `evidence/benchmarks/cut_reduction.csv` from `run_compare.py` showing >20% node reduction (experiment E6) |
| R6 | "exploit sparse matrix techniques, efficient numerical linear algebra" | [[Markowitz-1957-Elimination-Form-Inverse]] · [[Amestoy-1996-Approximate-Minimum-Degree]] · [[Davis-2004-Column-Approximate-Minimum]] · [[Sparse LU]] · [[Sparsity]] · [[Fill-Reducing Ordering]] | [[SparseBasis-LU]] `src/linalg/sparse_basis.cpp:81` (column-max pivoting only) · [[LDL-Factorization]] `perm_/pinv_` identity, no ordering · [[Canonicalizer]] CSC assembly | PARTIAL | CSC storage and sparse LU exist; **no Markowitz/AMD/fill-reducing ordering** anywhere in a factorization path; update-vs-refactor constants unjustified. | P1 — ordering + pivoting quality (09 §6.2 grades the fill-reducing part P2 if time-constrained) | `tests/sparse_basis_test` plus before/after fill (factor nnz) and time counters on an ill-conditioned Netlib basis |
| R7 | "multi-core parallelization" | [[Huangfu-2018-Parallelizing-Dual-Revised]] · [[Berthold-2019-Parallel-SCIP-UG]] · [[Eckstein-1994-Control-Strategies-Parallel]] · [[Work Stealing]] · [[Negative Parallel Scaling]] | [[ParallelTreeSearch]] `src/milp/parallel_tree_search.cpp:220` + `ThreadSafeNodeQueue` `include/markov_cero/milp/work_queue.hpp:18-57`; no stealing (`rg -i steal src/milp/` → 0) | REGRESSION | 4 threads measure **0.56× / 14% efficiency** (`evidence/benchmarks/phase4.json`); shared mutex heap with no load balancing. | **P0** RW-2 (ED-006): work stealing + subtree limits *or* default `--threads 1` and drop the speedup claim · P1 re-measure | `evidence/benchmarks/parallel_scaling.csv` (1/2/4/8 threads, experiment E5) ≥1× at 4 threads, else claim removed |
| R8 | "GPU acceleration considered where it provides measurable benefits" | [[Lu-2025-cuPDLP-GPU-Implementation]] · [[Unknown-2025-Overview-GPU-Based-First]] · [[Primal-Dual Hybrid Gradient]] · [[GPU Benefit Unproven]] · [[First-Order Accuracy Ceiling]] | [[GPU-PDHG-Engine]] `gpu/src/pdhg_step.cpp:357` · `scripts/run_gpu.py:241-245` never checks status · `evidence/benchmarks/crossover_study.csv` GPU loses 13/13 end-to-end | UNPROVEN | The PS precondition (measurable benefit) is unmet *and* the measurement is buggy: `BLEND → NumericalFailure` passes CI; no hardware recorded (PS-GAP-05). | **P0** RW-9 (ED-007): fix status check, record hardware, per-scale re-run, narrow the claim to what the data supports | Fixed `ctest gpu_benchmarks` + regenerated `evidence/benchmarks/crossover_study.csv` + `evidence/hardware.md` (experiment E4) |
| R9 | "emphasis is on numerical stability, scalability and reliable convergence" | [[Gleixner-2015-Iterative-Refinement-Linear]] · [[Renegar-1994-Condition-Numbers-Linear]] · [[Cline-1979-Estimate-Condition-Number]] · [[Ill-Conditioning]] · [[Numerical Stability]] | Tolerances centralized (`docs/decisions/ADR-M0-03`) · [[RuizScaling]] `src/scale/ruiz_scaling.cpp:20` · condition trigger `src/lp/dual/dual_simplex.cpp:405-411` · **no iterative refinement, no condition estimation** (0 hits) | PARTIAL | Policy and scaling are in place; the two standard numerical-*recovery* tools are absent, so R17 has no engine support. | P1 — RW-8: iterative refinement on LU solves + simple condition estimation (unblocks the R17 dossier cheaply) | New `tests/iterative_refinement_test`; refinement/condition counters emitted in the solution JSON |
| **Group C — Engineering constraints (R10–R14)** ||||||||
| R10 | "shall not be built upon any existing open source solver library" | [[Linderoth-2005-Noncommercial-Software-Mixed]] · [[Atamturk-2008-Integer-Programming-Software]] · [[Bixby-2020-Compiling-Mixed-Integer]] · [[COIN-OR-n.d.-CBC-Solver-Documentation]] (what "building upon" means) | `PROVENANCE.md` · `scripts/check-sovereignty.py` · empty `third_party/` · CI test `sovereignty_guard` | GOOD | Enforced mechanically. Comparing against HiGHS (ED-001) is *not* a C1 violation — comparison ≠ building upon ([[00-ground-truth]] A.6). | P2 — keep and publish the provenance chain in the demo | `ctest -R sovereignty_guard` green in CI; `PROVENANCE.md` + `readelf NEEDED` allowlist shown in demo pack |
| R11 | "refinery scheduling, crude blending, process optimization, production planning, logistics..." | [[Neiro-2004-Mathematical-Modeling-Petroleum]] · [[Pochet-2006-Production-Planning-Mixed]] · [[Kallrath-2002-Planning-Scheduling-Industry]] | `examples/refinery/*.mps`, `data/refinery`, refinery demo test | PARTIAL | Scope is met with synthetic examples; the dataset clause asks for "representative … case studies from open literature". | P2 — add 2–3 literature-derived case models for the demo | New `examples/cases/*.mps` + `ctest refinery_case_*` passing with verified JSON output |
| R12 | "thousands to millions of variables and constraints" | [[Sparsity]] · [[CSC Sparse Model]] · [[Bell-2008-Efficient-Sparse-Matrix]] · [[Hall-2005-Hyper-sparsity-Revised-Simplex]] | Two paths: dense gate `src/transform/canonicalize.cpp:8` (cap 8192) vs `src/transform/sparse_canonicalize.cpp:119`; simplex caps 1024×8192; largest evidence run ~5.6 s | PARTIAL | Arbitrary 2048×8192 threshold creates two divergent paths; reference-engine caps and no 1e5+ row instance in any evidence file. | P1 — RW-5 (ED-004): single sparse-first path, dense only as an internal fast path; add a 200k–1M instance set | `sparse_canonicalize_test` at scale + `evidence/scale_study/*` containing ≥1 instance with 1e5+ rows |
| R13 | "highly degenerate models, ill-conditioned matrices and difficult mixed-integer formulations" | [[Bartels-1968-Numerical-Investigation-Simplex]] · [[Maros-1993-Practical-Anti-Degeneracy]] · [[Goldfarb-1977-Practicable-Steepest-Edge]] · [[Degeneracy]] · [[Degeneracy Handling Gap]] · [[Bland-Only Pricing]] | [[RevisedSimplexEngine]] Bland default · [[DualSimplexEngine]] Harris `src/lp/dual/dual_simplex.cpp:209` but pricing is `tableau_norm`, *not* DSE (`dual_simplex.hpp:12-14`) | PARTIAL | Anti-cycling floor exists; the anti-degeneracy toolbox (steepest-edge, EXPAND/perturbation, repairs) and the hard-instance suite do not. | P1 — RW-7 dual steepest-edge with Bland kept as stall-detected fallback; the *demonstration* half is R17 (P0) | `tests/dual_simplex_test` DSE cases + degenerate suite (Klee–Minty-style, Netlib degenerate) timings |
| R14 | "basic application programming interface (API) or command-line interface is sufficient" | [[Multi-Engine Solver Architecture]] · [[Bixby-2020-Compiling-Mixed-Integer]] · [[Dolan-2002-Benchmarking-Optimization-Software]] (harnesses need a library entry point) | 3 CLIs; `apps/markov_cero_solve.cpp` is a 499-line monolith; library API = headers only, no `install()` | PARTIAL | The CLI half of R14 is met; the API half is nominal — orchestration cannot be tested or called outside `main`. | P1 — RW-4: move orchestration to `src/api/` (`solve_file()`, `solve_model()`), thin CLI, add `install()` | New `tests/api_test` linking the installed lib + a `cmake --install` smoke test |
| **Group D — Evaluation & standing (R15–R20)** ||||||||
| R15 | "standard benchmark problems from recognised optimization libraries such as MIPLIB, Netlib or Mittelmann" | [[Dolan-2002-Benchmarking-Optimization-Software]] · [[Mittelmann-n.d.-Mittelmann-LP-MILP]] · [[MIPLIB]] · [[Netlib LP Collection]] · [[Mittelmann Coverage Gap]] | `scripts/run_netlib.py`, `scripts/run_miplib.py`; `evidence/netlib_results.csv` (7) + `netlib_extended.csv` (5) + `miplib_results.csv` (3) | PARTIAL | Two of three named suites are used, thinly; **Mittelmann: 0 instances** (PS-GAP-04). | P1 — add Mittelmann LP + MILP sets to the harness | `evidence/mittelmann_results.csv` + a Mittelmann column in the comparison table |
| R16 | "solution quality and computational performance compared against at least one established commercial or open-source solver" | [[Dolan-2002-Benchmarking-Optimization-Software]] · [[Mittelmann Benchmarks]] · [[Geometric Mean Runtime]] · [[Missing External Baseline Comparison]] · [[No External Baseline]] · [[Lodi-2013-Performance-Variability-Mixed]] | **Zero comparison artifacts repo-wide** (search for highs/cplex/gurobi/cbc/scip across `evidence/`, `reports/`, `benchmarks/` → 0 hits); runners are single-solver | GAP | Binary evaluator criterion and the instrument for measuring every other P0 item; six phases shipped without it. | **P0 #1** — RW-3 / ED-001: `scripts/run_compare.py --baseline highs`, shared pre-registered instance set (step 1, [[13-restart-point]]) | `evidence/compare/compare_netlib.md` + `compare_miplib.csv`: status, objective, time, gap, geometric means, Dolan–Moré profile (experiment E1) |
| R17 | "A clear demonstration of numerical robustness should be provided" | [[Ill-Conditioned Instance Dossier]] · [[Ill-Conditioning]] · [[Degeneracy]] · [[Neumaier-2004-Safe-Bounds-Linear]] · [[KKT Residual]] | Degeneracy/ill-conditioning *unit tests* exist; no curated instance set, no report generator, no refinement/condition tooling | GAP | Tests prove code paths; nothing proves the *demonstration* the PS explicitly grades. | **P0** — dossier (step 5 of [[13-restart-point]]); P1 — RW-8 tooling feeds it | `evidence/robustness_dossier.md`: degenerate + ill-conditioned + weak-relaxation instances with per-instance status, time, residuals (experiment E3) |
| R18 | "transparent, extensible and sovereign foundation" | [[Kronqvist-2025-50-Years-Mixed-Integer]] · [[COIN-OR-n.d.-CBC-Solver-Documentation]] · [[Kumar-2010-Fifty-Years-Integer]] | Apache-2.0, 8 CI jobs, `PROVENANCE.md`; but doc drift: README claims, dangling `VERIFY.md` paths, stale `docs/history.md` | PARTIAL | The sovereign base is strong; claims that outrun the evidence (GPU headline, parallel speedup) erode transparency — evaluator risk K2. | P1 — RW-10: claims↔evidence table + R1–R20 coverage in STATUS · P2 doc sync | `rg` finds no uncorroborated speedup claim; `VERIFY.md` paths resolve; STATUS carries a copy of this matrix's Status column |
| R19 | "MIPLIB, Netlib LP, Mittelmann benchmark instances, QPLIB … case studies" | [[MIPLIB]] · [[Netlib LP Collection]] · [[Mittelmann Benchmarks]] · [[QPLIB]] · [[Bussieck-2026-mipfeas-Benchmark]] | `data/netlib`, `data/miplib` present; `data/qp` empty; `examples/refinery` = 1 demo; no Mittelmann | PARTIAL | 2 of 4 named sets ingested; QPLIB absent (R2 makes QP in-scope) and Mittelmann absent (see R15). | P2 — QPLIB subset only if the QP story needs it; Mittelmann is P1 under R15 | `data/qp/*.qps` + `evidence/qplib_results.csv` with convex/non-convex reported separately |
| R20 | "consistently deliver optimal or near-optimal solutions … within practical computation times" | [[Geometric Mean Runtime]] · [[Relative Optimality Gap]] · [[Mittelmann Benchmarks]] · [[Root-Only Cuts]] · [[Negative Parallel Scaling]] | Single-solver CSVs only; geometric mean never computed against anyone; root cuts 0.0% + parallel 0.56× | UNPROVEN | With no baseline the claim is unevidenced, and the measured cut/parallel results actively cut against the "faster than weaker implementations" half. | **P0** — folded into RW-3 (ED-001); the "faster" half depends on the R5/R7 fixes landing | Geometric-mean ratio table + per-instance gap columns in `evidence/compare/*`; MIPLIB primal-integral column |

## 21.2 P0 closure register (the six binary gaps)

Every row below is *binary* for the evaluator: either the artifact exists or the claim is
withdrawn. Order follows [[13-restart-point]]; the RW ids are from [[12-keep-remove-rebuild]].

| P0 item | RW / step | Decision note | Done when (artifact) | Fallback if it slips |
|---|---|---|---|---|
| R16 comparison harness | RW-3, step 1 | [[ED-001-comparison-harness-before-new-algorithms]] | `evidence/compare/compare_netlib.md` with geometric means + Dolan–Moré profile | none — this one has no fallback; it is the first build item |
| R4 interior-point + crossover | step 6 | [[ED-003-interior-point-required-by-ps]] | `tests/ipm_test` green + `evidence/ipm_crossover.md` | formal re-scope note in STATUS, approved before 2026-09-30 |
| R5 in-tree cut loop | RW-1, step 3 | [[ED-005-in-tree-cut-loop-not-more-cut-types]] | `evidence/benchmarks/cut_reduction.csv` >20% node reduction | report root-only honestly; withdraw any cut-effectiveness claim |
| R7 parallel load balancing | RW-2, step 4 | [[ED-006-load-balanced-parallel-or-demote]] | `evidence/benchmarks/parallel_scaling.csv` ≥1× at 4 threads | default `--threads 1`, delete speedup claims |
| R8 GPU measurement | RW-9, step 2 | [[ED-007-honest-gpu-scoping]] | fixed `run_gpu.py` status check + `evidence/hardware.md` + per-scale CSV | demote GPU to an appendix/engineering note |
| R17 robustness dossier | step 5 | [[ED-008-retain-zero-trust-verifiers]] (tooling: RW-8) | `evidence/robustness_dossier.md` covering degenerate, ill-conditioned, weak-relaxation | none — PS asks for it by name |

R20 has no row of its own: it is R16 (quality vs baseline) plus the "faster" half of R5/R7.

## 21.3 Research module map (which corpus modules feed which rows)

Modules `[M##]` are the headings of `docs/research_paper_references.md`; concept-layer entry
points are listed in [[Research MOC]] §Layers.

| Module | Topic | Feeds rows |
|---|---|---|
| M0 | orientation / survey / what a solver core is | R1, R10, R18 |
| M1 | input, scaling, canonicalization | R6, R12 |
| M2 | sparse numerical linear algebra | R6, R12 |
| M3 | simplex (primal revised, dual, degeneracy) | R4, R5, R13 |
| M4 | interior-point methods + crossover | R4 |
| M5 | presolve / preprocessing | R5, R13, R20 |
| M7–M10 | branch-and-bound, cuts, heuristics, branching | R5, R20 |
| M11 | numerical stability, ill-conditioning, certificates | R9, R13, R17 |
| M12 | parallel and GPU execution | R7, R8 |
| M13 | benchmarking, datasets, comparison methodology | R15, R16, R19, R20 |
| M14 | industrial case models (refining, planning, logistics) | R11, R19 |
| M15–M16 | future extensions (NLP/MINLP), learning-to-branch | R3, R5 (deferred — see [[ED-009-defer-ml-branching]]) |

## 21.4 Research findings → Engineering decisions

Each durable decision this audit forces has its own note in `docs/research/engineering-decisions/`.

| # | Finding (research) | Req | Component | Implementation | Metric |
|---|---|---|---|---|---|
| 1 | No external baseline exists; single-run tables are not evidence ([[No External Baseline]], [[Lodi-2013-Performance-Variability-Mixed]]) | R16, R20 | evaluation layer (greenfield) | [[ED-001-comparison-harness-before-new-algorithms]] / RW-3 → `scripts/run_compare.py` | geometric-mean ratios + Dolan–Moré profile |
| 2 | IPM without crossover is not R4 ([[No Interior-Point Engine]], [[No Crossover]]) | R4, R5 | engine layer | [[ED-003-interior-point-required-by-ps]] → sparse IPM + crossover (step 6) | LP solved + basis certified by [[IndependentVerifiers]] |
| 3 | Root-only separation prunes nothing ([[Root-Only Cuts]], [[Cut Efficiency]]) | R5, R20 | [[BranchAndCut]] + [[CutGenerators]] | [[ED-005-in-tree-cut-loop-not-more-cut-types]] / RW-1 → in-tree loop + bounded [[Cut Pooling]] | cut-node reduction >20% |
| 4 | A central mutex queue scales negatively ([[Negative Parallel Scaling]], [[Work Stealing]]) | R7 | [[ParallelTreeSearch]] | [[ED-006-load-balanced-parallel-or-demote]] / RW-2 → stealing + subtree limits, or demote | 1/2/4/8-thread speedup ≥1× at 4 |
| 5 | GPU first-order wins are size-dependent and often uncorroborated ([[GPU Benefit Unproven]], [[Lu-2025-cuPDLP-GPU-Implementation]]) | R8 | [[GPU-PDHG-Engine]] | [[ED-007-honest-gpu-scoping]] / RW-9 → status-check fix + per-scale re-run | end-to-end speedup with `evidence/hardware.md` |
| 6 | Degenerate LPs need pivot quality, not only anti-cycling ([[Steepest Edge]], [[Harris Ratio Test]], [[Bland-Only Pricing]]) | R13, R9 | [[DualSimplexEngine]] | RW-7 → dual steepest-edge, Bland demoted to fallback | degenerate-suite solve rate and time |
| 7 | Numerics need recovery + certification ([[Ill-Conditioning]], [[Iterative Refinement]], [[KKT Residual]]) | R9, R17 | linalg / numerics | RW-8 → refinement on LU solves + condition estimation | post-refinement residual; dossier pass rate |
| 8 | Presolve is the multiplicative speedup ([[Bixby-2002-Evolution-of-LP]], [[Achterberg-2020-Presolve-Reductions-Mixed]]) | R5, R13, R20 | [[Presolve]] | [[ED-010-presolve-depth-over-new-engine]] / RW-6 → implied bounds, forcing rows, probing-lite | presolve nnz/row reduction + solve-time delta |
| 9 | Sparsity is a prerequisite for R12 scale ([[Sparsity]], [[CSC Sparse Model]]) | R6, R12 | [[Canonicalizer]] | [[ED-004-sparse-first-canonicalization]] / RW-5 → one sparse-first path | ≥1e5-row instance solved within caps |
| 10 | Verification must be independent of solver status strings ([[KKT Residual]], [[Unknown-2026-Verified-Linear-Programming]]) | R17, R18 | [[IndependentVerifiers]] | [[ED-008-retain-zero-trust-verifiers]] → certificates in compare/demo output | `verified=true` + KKT residual columns in every table |
| 11 | ML branching needs corpora + an eval harness ([[Giallombardo-2025-Machine-Learning-Techniques]], [[Zhang-2025-Learning-Select-Nodes]]) | R5, R18 | branching | [[ED-009-defer-ml-branching]] → roadmap P3 only, delist README Phase 7 | README makes no ML claim; classical branching measured instead |
| 12 | Clean-room provenance is the only *provable* R10 story (09 §6.5) | R10, R18 | CI | KEEP `sovereignty_guard` ([[12-keep-remove-rebuild]] §8.1) | `ctest -R sovereignty_guard` green in all 8 CI jobs |

## 21.5 Navigation — walking the graph in both directions

**Research → Code (start from a question about "what should exist"):**
1. [[Research MOC]] → [[cross-paper-synthesis]] §8 (the 12-item collective-knowledge verdict, each item already tagged with R numbers).
2. Follow the R tag into this matrix (§21.1) → read Status, Gap, Recommendation, Verification for that R.
3. From the Recommendation column go to [[12-keep-remove-rebuild]] (RW-1…RW-10 rows) and [[13-restart-point]] (step order).
4. From the Current-code column open the component note stem ([[BranchAndCut]], [[Presolve]], …) in `docs/codebase/components/`, which carries exact `file:line` evidence.
5. Reverse direction from a paper: open the paper note in `docs/research/papers/`, read its "R" / technique links, then jump up this matrix.

**Code → Research (start from a file, ask "why is it shaped like this"):**
1. Source file → the component note that lists it (`source_files:` frontmatter + Implementation Facts).
2. Component note → its **Research Justification** section (wikilinks to `algorithms/`, `concepts/`, `techniques/`) or its **Open Questions** for gaps.
3. Algorithm/concept note → the paper notes it cites, plus the synthesis rows in [[cross-paper-synthesis]] §1–§7.
4. Gaps surface as `research-gaps/` notes ([[No Interior-Point Engine]], [[Missing External Baseline Comparison]], …) which map back to this matrix's Status column.
5. After any edit run `python3 scripts/link_backlinks.py` so "Referenced By" backlinks regenerate and the graph stays walkable.

**Where the MOC lives:** [[Research-Code Traceability MOC]] will be authored separately (MOCs are
another writer's deliverable) and will live beside [[cross-paper-synthesis]] and
[[research-dependency-map]] in `docs/research/maps/`. This file is its backing table — the MOC
links here, here links to [[00-ground-truth]] (ground truth), [[09-research-code-alignment]]
(status source), [[12-keep-remove-rebuild]] (actions) and the ten
`docs/research/engineering-decisions/ED-0NN-*` notes. The MOC link above is reserved and will
resolve once that note is created.

## 21.6 Rollup (recomputed from [[09-research-code-alignment]] §6.1 rows)

Note: 09 §6.7 prose says "4 GOOD, 10 PARTIAL" while its own §6.1 table grades 3 GOOD and
11 PARTIAL; the table is authoritative and is what this matrix copies.

| Status | Count | Requirements |
|---|---|---|
| GOOD | 3 | R1, R2, R10 |
| PARTIAL | 11 | R3, R5, R6, R9, R11, R12, R13, R14, R15, R18, R19 |
| GAP | 3 | R4, R16, R17 |
| REGRESSION | 1 | R7 |
| UNPROVEN | 2 | R8, R20 |
| **P0 items** | **6** | R4, R5 (cut loop), R7, R8, R16, R17 (+ R20 via R16) |
