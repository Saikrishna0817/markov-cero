---
type: research-synthesis
tags: [maps, research-synthesis, cross-paper]
status: stable
verified_on: 2026-09-25
---

# Cross-Paper Synthesis — what the 204-reference corpus collectively says

> Synthesis across the 17 modules of `docs/research_paper_references.md` and the concept layer in
> `docs/research/`. Read after [[Research MOC]]; it does not replace individual notes.

**Citation legend.** `[M##]` = module number in `docs/research_paper_references.md`;
`[E###]` = entry number in that same list. Wikilinks point at existing notes only. Anything not
directly attested by a note or by `docs/audit/00-ground-truth.md` is prefixed **Inference:**.

---

## 1. Repeated techniques (appear across many modules)

| Technique | Modules where it recurs | Why it matters |
|---|---|---|
| **Sparsity exploitation** (CSC/CSR, sparse LU, fill-reducing ordering, hyper-sparsity) | M2 sparse algebra, M3 simplex, M4 IPM, M12 GPU, M1 input/scaling | The same data structure serves every engine: [[Sparsity]] → [[Sparse LU]] → [[Fill-Reducing Ordering]]. M2 alone carries 17 entries (Bartels–Golub [E12], Forrest–Tomlin [E13], Gilbert–Peierls [E14], AMD [E15], COLAMD [E16], Markowitz [E17]); Hall & McKinnon report 5.2× from hyper-sparse FTRAN/BTRAN loops [E35]. Nothing downstream (simplex, IPM, ADMM, GPU SpMV) works at R12 scale without it. |
| **Warm starts / basis reuse** | M3 simplex, M5 presolve (postsolve re-solve), M7 B&B, M9 heuristics, M10 branching | Every child node inherits a parent basis; every cut insertion re-solves an LP. [[Warm Start]] is why [[Dual Simplex]] exists as the node workhorse (Koberstein [E37]); feasibility-pump and diving iterations are cheap only because of it (Fischetti et al. [E110], Berthold [E115]). |
| **Scaling / equilibration** | M1 input, M4 IPM (Gondzio [E56]), M5 presolve (Achterberg et al. [E65] order it explicitly), M11 numerics | [[Scaling]] is called "the cheapest robustness win" in [[Ill-Conditioning]]: it changes κ(A) and the meaning of every absolute tolerance without changing the mathematics (Curtis & Reid / Oren [E10, E73]; Ruiz-style in [[Ruiz Scaling]]). |
| **Presolve + reversible postsolve** | M0 survey, M5 presolve, M7 architecture, M9 presolve heuristics | Bixby 2002 [E1] attributes one of the four big multiplicative LP speedups to presolve; Achterberg 2007 [E3] makes it a module boundary. [[Presolve]] is the only component every surveyed solver has and none can remove. |
| **Anti-cycling + tolerance hygiene** | M3 simplex, M11 numerics | [[Bland Anti-Cycling]] [E33], [[Harris Ratio Test]] [E32], EXPAND [E44, E143] recur as a *pair*: a finite-termination floor plus a performance-tolerant acceptance rule. Repeated in Koberstein [E37], Gill et al. [E143], Georg & Hettich [E150]. |
| **Component ablation as evaluation method** | M0 survey, M7 architecture, M13 benchmarking | Bixby [E1] and Achterberg et al. [E83] both justify features by switching one on/off and measuring. This is the only style of claim that transfers to R16. |
| **Independent verification / certificates** | M11 numerics, M13 benchmarking | Farkas rays [E149], KKT residual checks, MIPLIB solution checkers [E187] — verification is repeated across modules because solver-reported status strings are not evidence ([[KKT Residual]]). |

## 2. Contradictory approaches (the corpus disagrees with itself)

- **Simplex-degeneracy camp vs first-order-GPU camp.** Goldfarb & Reid [E31], Goldfarb & Forrest [E38],
  Harris [E32], Koberstein [E37] and Maros [E152, E161] all argue that *pivot quality* (steepest edge,
  Harris tolerances, bound-flipping, positive-edge pricing) is what makes degenerate LPs converge — see
  [[Steepest Edge]], [[Degeneracy]]. The GPU line (Lu & Yang cuPDLP [E177], the GPU first-order survey
  [E178], PDCS [E176], Bell & Garland [E171]) argues the opposite allocation: throughput of
  bandwidth-bound first-order iterations beats per-pivot cleverness on *large* LPs.
  Both are right about their own regime: [[First-Order Accuracy Ceiling]] records the accuracy floor of
  the GPU camp; [[Bland-Only Pricing]] records the quality floor of the current simplex camp.
- **Cut-heavy vs presolve-heavy.** Balas et al. [E95] report 86% vs 55% solve rate from revived Gomory
  cuts; Marchand & Wolsey [E99] and Balas & Zemel [E100] push MIR/cover families; Turner et al. [E104]
  argue cut *management* is where modern gains live. Against that, Achterberg et al. [E65] and Bixby [E1]
  concentrate the win in presolve, and Savelsbergh [E63] in probing. **Inference:** the disagreement is
  partly historical (each camp measured with the other component off); [[Achterberg-2005-General-Mixed-Integer]]
  is the reconciliation — both are large, and they interact.
- **Centralized vs decentralized parallel control.** Eckstein [E163] (centralized control, rendezvous,
  incumbent broadcast) vs Zhang et al./ParaSCIP [E165] and PIPS-PSBB [E167] (work stealing, decentralized
  rebalancing) vs Lai & Sahni [E173] who show parallelism can make B&B *strictly worse*. See
  [[Work Stealing]], [[Negative Parallel Scaling]].
- **Normal equations vs symmetric indefinite LDLᵀ inside IPM.** Vanderbei/Fourer–Mehrotra [E55] frame it
  as a conditioning tradeoff; Lustig et al. [E50] document dense-column Schur instability. The corpus does
  not pick a winner. **Inference:** this is a per-instance dispatch question, not a global choice.
- **Active-set QP vs first-order/ADMM QP.** Goldfarb & Idnani [E74, E75] (dual active-set, Cholesky/QR
  updates, no Phase I) vs Wright/Monteiro–Adler IPM-QP [E76] vs [[ADMM]]-style splitting, which is what
  markov-cero ships ([[QPLIB]] note records the dimension cap).
- **"Cuts inside the tree" vs "root-only is enough".** Padberg [E98], Cornuéjols [E84] and Balas et al.
  [E95] separate at integer solutions/nodes; [[Root-Only Cuts]] documents the current repo state and its
  measured 0.0% node reduction.

## 3. Complementary techniques that should be combined

- **IPM + crossover + dual simplex warm start.** [[Interior-Point Method]] gives iteration counts
  insensitive to degeneracy; [[Crossover]] (Ye [E58]) turns the interior point into a vertex + basis; the
  warm-started [[Dual Simplex]] consumes that basis at MIP nodes. Wright [E2] fixes the division of labor;
  the repo has the simplex half and neither of the other two ([[No Interior-Point Engine]],
  [[No Crossover]]).
- **Presolve → scaling → root cuts → tree cuts → postsolve, in that order.** Achterberg et al. [E65]
  specify phase ordering and rule short-circuiting; Gondzio's IPM presolve [E56] and Ruiz-style scaling
  must agree on order or dual recovery breaks ([[Presolve-Postsolve Stack]], [[Scaling]]).
- **Steepest edge + Harris + EXPAND/perturbation + Bland-as-fallback.** None of these substitutes for
  another: Goldfarb & Forrest [E38] supply pivot quality, Harris [E32] supplies acceptance width, Gill et
  al. [E44/E143] supply anti-stalling, Bland [E33] supplies the guarantee. [[Bland-Only Pricing]] states
  the correct architecture explicitly (good pricing default, Bland demoted to stall-detected fallback).
- **Root cuts + in-tree separation + a bounded cut pool.** Balas et al. [E95] plus Marchand & Wolsey
  [E102] plus Rex/Gleixner's pooling question [E109] compose into a cut manager; see [[Cut Efficiency]],
  [[Cut Validity]].
- **Work stealing + deterministic reduction.** Parallel B&B needs decentralized queues ([E163, E165])
  *and* reproducible runs for benchmarking ([E172]) — see [[Work Stealing]], [[Deterministic Reduction]].
- **GPU first-order above a size threshold, simplex below it.** Lu & Yang [E177] lose on small instances
  by their own account; the repo's `evidence/benchmarks/crossover_study.csv` reproduces that shape
  ([[GPU Benefit Unproven]]). **Inference:** the combination is an engine-dispatch policy in
  [[Multi-Engine Solver Architecture]], not a single winner.

## 4. Common datasets / benchmarks / metrics

| Kind | Names | Notes |
|---|---|---|
| MIP instances | [[MIPLIB]] (MIPLIB 2017 = 1065 collection + 240 benchmark [E180]; MIPLIB 2003 taxonomy [E181]; D-MIPLIB [E189]) | Where degenerate/weak formulations concentrate — the R13 territory. |
| LP instances | [[Netlib LP Collection]] [E184] (AFIRO, E226, PILOT, DFL001…) | The classic correctness/robustness set; also the set IPM crossover papers use. |
| Leaderboards | [[Mittelmann Benchmarks]] [E183, E190] | Live, geometric-mean based, matched hardware — the shape an evaluator recognises. |
| QP instances | [[QPLIB]] (named in R19) | Needed because R2 makes QP in-scope; convex vs non-convex results must be reported separately. |
| Heuristic sets | mipfeas [E191] | For primal-heuristic quality claims (Spoorendonk [E123] reports +23.7% primal integral). |
| Aggregation | [[Geometric Mean Runtime]] (GM of ratios, penalty time for timeouts), Dolan–Moré profiles [E185] | Arithmetic means are explicitly rejected by the corpus. |
| Quality metrics | [[Relative Optimality Gap]], [[KKT Residual]], [[Primal Residual]], [[Cut Efficiency]], [[Parallel Speedup]] / [[Parallel Efficiency]] | Time without accuracy is not comparable — report both. |
| Experimental design | Lodi & Tramontani [E182] multi-seed/permutation runs; Schweizer deterministic parallel [E172] | Single-run tables are not evidence; the repo's current CSVs are single runs. |

## 5. Repeated limitations (what even strong solvers fail at)

- **Ill-conditioning and degeneracy are never "solved", only managed** — Renegar [E148], Cline et al.
  [E144], Charnes/Megiddo [E141], Maros [E156], Neumaier & Shcherbina [E153]: every numerics entry in M11
  is a mitigation with a failure mode of its own (weight drift, tolerance-induced infeasibility,
  over-aggressive presolve). See [[Ill-Conditioning]], [[Degeneracy]].
- **First-order methods hit an accuracy ceiling** — [[First-Order Accuracy Ceiling]]; residual stagnation
  ≈ conditioning × ε × scale, with no refinement mechanism (Gleixner et al. [E147] is the simplex-side fix).
- **Parallel B&B does not scale for free** — Lai & Sahni [E173] anomalies, plus granularity: Amdahl from a
  serial root phase. See [[Negative Parallel Scaling]], [[Parallel Scalability Gap]].
- **GPU first-order wins are size-dependent and often uncorroborated** — [E177, E178] lose on small LPs;
  the repo's own study is <1× in 13/13 rows ([[GPU Benefit Unproven]]).
- **Performance variability defeats single-run claims** — Lodi & Tramontani [E182].
- **Cuts add rows and destroy sparsity** — Balas et al. [E95] and Rex/Gleixner [E109]: unbounded separation
  costs more than it prunes; budgets are mandatory.
- **Presolve can actively hurt** — Achterberg et al. [E65]: aggressive reductions degrade conditioning and
  risk wrong answers beyond tolerance.
- **No solver covers everything** — Wright [E2] (barrier has no basis), Achterberg [E3] (components trade
  off), Kumar et al. [E4] (fifty years, still no single method).

## 6. Ideas that should NOT be combined (for this project)

- **ML branching / ML cut selection (module 16: [E202–E204], Zhang et al. node selection [E139],
  Giallombardo et al. [E107]) inside a 5-day window.** Ground-truth C5 sets the idea-submission deadline at
  2026-09-30; training data, a harness and a fallback rule do not fit, and R5's list does not mention ML.
- **Exact/rational arithmetic in the inner pivot loop** ([E155, E157, E158, E160]) — cost per pivot is
  multiplied by every node; keep it as an offline certificate path at most.
- **Aggressive presolve + aggressive cuts + no postsolve verification.** Each is individually risky
  ([E65] warns on conditioning; [E95] on sparsity); stacked, errors compound in original-space recovery
  ([[Presolve-Postsolve Stack]]).
- **Replacing Bland anti-cycling with steepest edge.** The corpus never suggests removal — only demotion
  ([[Bland-Only Pricing]]). Dropping the guarantee converts a performance problem into a non-termination bug.
- **Claiming GPU benefit from kernel-only timings or from a run whose status was never checked.**
  Ground-truth C.3 records `BLEND → NumericalFailure` passing a test because the runner skipped the status
  check; R8 makes benefit a precondition ([[GPU Benefit Unproven]]).
- **Aggregating across different instance sets** (comparing a Netlib geometric mean with a MIPLIB one) —
  [[Geometric Mean Runtime]] forbids it explicitly.
- **A central mutex work queue + claims of parallel speedup.** Measured 0.56× at 4 threads; adding threads
  without redesigning distribution is negative work ([[Negative Parallel Scaling]]).
- **Comparing against a solver only on instances we chose after seeing the results.** Pre-registration is
  required by [[Mittelmann Coverage Gap]] methodology notes and by Lodi & Tramontani [E182].

## 7. Open research gaps relevant to this project

- [[No Interior-Point Engine]] — R4 half-missing; PDHG is a different algorithm class (PS-GAP-06).
- [[No Crossover]] — no basis extraction, so no IPM→MIP handoff, no sensitivity, no reduced-cost fixing from it.
- [[Missing External Baseline Comparison]] / [[No External Baseline]] — R16 binary, PS-GAP-03.
- [[Mittelmann Coverage Gap]] — named benchmark set absent, PS-GAP-04.
- [[Ill-Conditioned Instance Dossier]] — R17 demonstration does not exist yet.
- [[Degeneracy Handling Gap]] — anti-cycling floor exists; anti-degeneracy toolbox and measurement do not.
- [[Parallel Scalability Gap]] — R7 currently a measured regression (0.56×).
- [[GPU Benefit Unproven]] — R8 fails its own "measurable benefit" precondition today.
- **Inference (not a named gap note):** QPLIB coverage is absent from `evidence/`, though R19 names it —
  tracked inside [[QPLIB]] rather than as a separate research-gaps note.

## 8. Collective knowledge verdict

**The research set, taken together, says a credible solver needs:**

1. A sparse CSC/CSR canonical model and sparse LU basis handling with fill-reducing ordering and a
   refactorization policy — [[Sparsity]], [[Sparse LU]], [[Markowitz Pivoting]] → **R6, R12**.
2. A warm-start-capable dual revised simplex as the node-LP workhorse, plus a revised primal simplex for
   cold solves — [[Dual Simplex]], [[Revised Simplex]], [[Warm Start]] → **R4, R5**.
3. A primal-dual interior-point engine *with* crossover to a basis; IPM without crossover is not R4 —
   [[Interior-Point Method]], [[Crossover]], [[No Interior-Point Engine]] → **R4, R5**.
4. A multi-rule presolve pipeline (probing, dual fixing, aggregation, implied bounds) with a reversible
   postsolve stack — [[Presolve]], [[Achterberg-2020-Presolve-Reductions-Mixed]] → **R5, R20**.
5. Uniform scaling/unscale across every engine so absolute tolerances keep their meaning —
   [[Scaling]], [[Ruiz Scaling]] → **R9, R13**.
6. A degeneracy toolbox: Harris tolerances, dual steepest-edge pricing, EXPAND/perturbation, Bland as
   stall-detected fallback — [[Steepest Edge]], [[Harris Ratio Test]], [[Degeneracy Handling Gap]] → **R13, R17**.
7. Numerical recovery and certification: condition estimation, iterative refinement, independent
   KKT/Farkas verification of every terminal result — [[Ill-Conditioning]], [[Iterative Refinement]],
   [[KKT Residual]] → **R9, R17**.
8. Branch-and-cut with separation *inside* the tree under per-node cut budgets and a cut pool —
   [[Branch and Cut]], [[Root-Only Cuts]], [[Cut Efficiency]] → **R5, R20**.
9. Primal heuristics that produce incumbents early (feasibility pump, rounding, diving/RINS) and are
   measured by time-to-first-incumbent — [[Feasibility Pump]], [[Diving]], [[Rounding Heuristic]] → **R5, R20**.
10. Reliability branching with screened candidates and justified probe budgets (η_rel=8, λ=4 as starting
    defaults, not constants) — [[Pseudo-Cost Branching]], [[Strong Branching]],
    [[Achterberg-2005-Branching-Rules-Revisited]] → **R5, R16**.
11. Parallelism that is designed in (decentralized queue/work stealing, deterministic mode, in-simplex
    parallel pricing/factorization) and a GPU path only where per-scale end-to-end evidence shows benefit —
    [[Work Stealing]], [[Negative Parallel Scaling]], [[GPU Benefit Unproven]] → **R7, R8**.
12. An evaluation layer: ≥1 external baseline, geometric-mean ratios + Dolan–Moré profiles, multi-run
    dispersion, and MIPLIB/Netlib/Mittelmann/QPLIB coverage — [[Geometric Mean Runtime]],
    [[Missing External Baseline Comparison]], [[MIPLIB]] → **R15, R16, R19**.

**Inference:** items 1–5 and 7 are infrastructure, 6 and 8–10 are the MILP performance core, 11 is
conditional on measurement, and 12 is the part SIH grades directly — the audit's section D reaches the
same ordering from the code side.
