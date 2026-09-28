---
type: audit
phase: 19
title: Competitive Landscape — SIH26119 rival solver survey
tags: [audit, competitive, sih, r16, landscape]
status: complete
date: 2026-09-25
---

# 19. Competitive Landscape — markov-cero vs the SIH26119 field

> **Report-only note** (scope decision: no edits to [[15-roadmap]] or [[17-sih-demo-strategy]]).
> Answering two questions: *who else is building this, and what did they miss that we can own?*
> Evidence labels: **[Observed]** = file:line in a cloned repo · **[Claimed]** = README/marketing only ·
> **[Not found]** = searched, absent. All competitor repos cloned 2026-09-25 to
> `/tmp/opencode/competitors/` for line-level verification; per-repo deep notes live in
> [[Competitive Landscape MOC]].

## 19.0 Method & inventory

**Discovery.** GitHub search (`q=SIH26119`, `topic:sih2026 solver`, `"indigenous" optimization
solver`, `“optimization solver” SIH 2026`) found **27 relevant repos**; all were cloned and
inspected locally (two needed name resolution via profile pages: `xarjunpatil`,
`RaghavGupta2910`). Established-solver and prior-art columns came from official docs,
Mittelmann benchmark pages, and arXiv/README sources of the named projects.
`Saikrishna0817/markov-zip1` was excluded — it is our own `origin` remote.

**Tiers.** T1 = real solver engines + evidence or CI (9 projects, 8 public repos);
T2 = partial engines (8); T3 = pitch/empty (10).

| Tier | Repo (cloned dir) | Lang | Own LOC | CI | One-line verdict |
|---|---|---|---|---|---|
| T1 | `SANKHYA` — thegoodengineers/SANKHYA | C++ | ~139k (incl. tests) | 6 jobs + guard | IPM+parallel+cuts+CI+ldd sovereignty gate; **the complete package** [Observed] |
| T1 | `Deekshith2205__sih-26` | C++ | 32.6k | 6 jobs + guard | **Same SANKHYA project, second snapshot** (192 shared paths, same generator) [Observed] |
| T1 | `sankhya` — team-vertexx/sankhya | C++ | 25.4k | 2 jobs | CI *runs* Netlib vs published optima; honest "62% of HiGHS" self-grade [Observed] |
| T1 | `IGAOS` — Lothnic/IGAOS | C++/CUDA | 8.5k | 2 jobs | PyPI + pinned HiGHS baseline; **README numbers contradict own CSVs** [Observed] |
| T1 | `trijalpgunaseelan__Igaos-public` | C++ | 17.8k | none | Broadest engine set + CPLEX/Gurobi CSVs; claims CI it doesn't have [Observed] |
| T1 | `refinery-optimizer` — kavinR-11 | C++ | 17.0k | **none** | Only rival publishing an HiGHS *win*; inputs missing, no CI [Observed] |
| T1 | `PIPEPYE` — Satyanshgaur | C++/CUDA | 33.2k | CPU+CUDA | Per-instance HiGHS ratios where **HiGHS wins 18/32** [Observed] |
| T1 | `VX03` — VioniX37 | Python | 14.6k | 3 jobs | HSD-IPM + PyTorch GPU + CI bench job; PolyForm Strict licence [Observed] |
| T1 | `Firefly_solver` — akshayvarma121 | C++/CUDA | ~3k (rest is build junk) | none | Real CUDA LP+MILP; "HiGHS reference" = 2 hardcoded numbers [Observed] |
| T2 | `RaghavGupta2910__optimisation_solver` | C++ | 25.8k | none | Solid prototype, no IPM/GPU, zero committed results [Observed] |
| T2 | `JosephXpanakaL__bharatopt` | C++ | 5.2k (+24.8k vendored) | broken | Broad incl. CUDA; "simplex" is a facade, zero CSVs [Observed] |
| T2 | `AryanMotiani__sovereign-solver` | Python | 15.6k | none | Full engine set; evidence is 4 Netlib rows incl. one wrong [Observed] |
| T2 | `Kouhsik33__Bharat-Opt` | C++/CUDA | 8.5k | none | PDHG-only; README: "not been built, run, or verified by a human" [Observed] |
| T2 | `mohitsaitummalapalli-tech__…` | C++ | 11.4k | none | Parsers+LU+tests only; algorithms dirs are README stubs [Observed] |
| T2 | `Mage-100__nomos` | C++ | 1.8k | none | Header-only two-phase simplex; tests disabled [Observed] |
| T2 | `ayushmishra2992__ApexCUDA-Optimization` | Python | 3.0k | none | ADMM+B&B+MPS; byte-identical README to ApexCUDA_Airline [Observed] |
| T2 | `hemasri-152006__BharatOpt` | Python | 1.7k | none | Real 1,115-line simplex but broken imports; no B&B [Observed] |
| T2 | `AashnaDas__ApexCUDA_Airline` | Python/nb | 0.2k | none | Pitch: 35.79× is one SpMV; backend fakes telemetry [Observed] |
| T3 | `infinity390__…2026_119` | Python | 0.4k | none | Exact Fraction vertex enumeration on toys [Observed] |
| T3 | `xarjunpatil__SIH26119` | Python/HTML | 0.1k | none | Dashboard pitch; badges even say "Landslide GIS" (template reuse) [Observed] |
| T3 | `Abhinav-Prabhakar__SIH2026` | — | 0 | none | Scrape of all 175 SIH problem statements, no solver [Observed] |
| T3 | `NIVION-HUB__SIH2026-PS`, `AryanSahu321__sih` | — | 0 | none | Problem-statement text / scoring matrix only [Observed] |
| T3 | `shivarajhdindure-tech__…` | — | 0 | none | 29 scaffold files, **every one 0 bytes** [Observed] |
| T3 | `Nandhitha-ai__…`, `Naresh-V-7__…`, `sheena9937__…` | — | 0 | none | 32-byte README or zero commits [Observed] |

**Brand clutter (an evaluation risk in itself)** [Observed]: three distinct projects brand
themselves **SANKHYA** (thegoodengineers, Deekshith2205 mirror, team-vertexx), two brand
**IGAOS** (Lothnic, trijalpgunaseelan — zero byte-identical files, so siblings not forks),
three brand **BharatOpt**, two brand **ApexCUDA**. Evaluators skimming GitHub will collide
on names; our distinct `markov-cero` identity + provenance trail is a quiet advantage.

## 19.1 Deep peer matrix — T1 vs markov-cero

| Capability (PS req.) | **markov-cero (us)** | SANKHYA (thegoodeng.) | team-vertexx sankhya | Igaos-public | refinery-opt | VX03 | IGAOS (Lothnic) | PIPEPYE | Firefly |
|---|---|---|---|---|---|---|---|---|---|
| Revised simplex R4a | ✅ primal+dual, Harris [09] | ✅ | ✅ 82/88 Netlib [Obs] | ✅ | ✅ steepest-edge/Devex | ✅ py | ✅ pr+du | ✅ dual+Devex | ✅ primal only |
| **Interior-point R4b** | ❌ papers only | ✅ Mehrotra+Gondzio, 130 KB `src/ipm/ipm.cpp:400` | ❌ | ✅ `ipm.cpp` 692 L | ✅ `ipm_solver.cpp:178` | ✅ HSD | ❌ | ❌ | ❌ |
| Crossover | ❌ | ✅ on by default `options.cpp:1344` | ⚠️ PDHG→basis polish only | ✅ | ✅ `crossover.cpp` | ✅ | ❌ | ❌ | ❌ |
| MILP depth R5 | ✅ B&B + root cuts + heuristics | ✅ root GMI **on** `options.cpp:993`, parallel MIP | ✅ branch-cut, cover/c-MIR/Gomory | ✅ B&C GMI/MIR | ✅ B&B + root GMI | ✅ B&C | ⚠️ root-only Gomory, 5% drop-gate, no MIP presolve | ✅ B&B warm-start | ⚠️ B&B, no cuts |
| Presolve/postsolve | ✅ 3-rule + Ruiz, LIFO | ✅ + dual postsolve | ✅ 12 reductions + dual postsolve | ✅ | ✅ reversible stack | — | ✅ LP-only | ✅ 5-pass + postsolve | ✅ 15 KB |
| QP R2 | ✅ ADMM+LDLᵀ (+MIQP) | ✅ convex QP + MIQP | ✅ ADMM | ✅ MIQP | ✅ | ✅ IPM+active-set | ✅ ADMM QP | ❌ | ❌ claimed, **absent** |
| **GPU R8** | ✅ 4 CUDA kernels (loses 13/13 measured) | ✅ CUDA PDHG, honest 1.4–6.5× | ✅ T4 3.1–12× | ⚠️ CUDA present, "not executed" README:51 | ⚠️ FP32-only `cuda_pdhg.cu:17` | ✅ PyTorch CUDA | ✅ `pdhg.cu` RTX 4060 | ✅ sm_86 | ✅ `pdlp_cuda.cu` sm_89 |
| **Parallel R7** | ⚠️ jtree exists, **0.56× regression** | ✅ OpenMP + `mip_threads` | ✅ own thread pool | ✅ OpenMP | ✅ thread-pool B&B + OpenMP | ⚠️ racing (py) | ❌ serial `while(true)` | ✅ OpenMP sparse | ❌ |
| CI | ✅ `ci.yml` | ✅ TSan+fuzz+ASan+netlib+**ldd gate** `ci.yml:808` | ✅ incl. Netlib verify job `:52` | ❌ (claims it) | ❌ | ✅ pytest+HiGHS bench | ✅ weak (build+wheel) | ✅ CPU+CUDA | ❌ |
| **R16 baseline** | ❌ **zero artifacts** | ✅ HiGHS separate proc, CSVs, "2.62× slower" `:1301` | ✅ inside CI, "62% of HiGHS" | ✅ CPLEX 13/13, Gurobi 21/22 | ✅ JSON, "80% MILP win" (5 inst) | ✅ CI job vs highspy | ✅ pinned HiGHS 1.15.1 + Gurobi | ✅ 32 rows, HiGHS wins 18 | ❌ label only |
| Sovereignty R10 | ✅ CI guard + `PROVENANCE.md` | ✅ **ldd fail-on-solver** `:814` | ✅ HiGHS external only | ✅ bench-only imports | ✅ regex scanner (line-based) | ⚠️ highspy/gurobipy in `requirements.txt` | ✅ `DEPENDENCIES.md` | ✅ policy `environment.md:43` | ✅ Eigen only |
| Licence | Apache-2.0 | ⚠️ SPDX, **no LICENSE file** | Apache-2.0 | ⚠️ custom, `<TEAM NAME>` unfilled | ⚠️ **no LICENSE file** (README says MIT) | ❌ PolyForm Strict (non-OSI) | MIT | ⚠️ **no LICENSE file** | MIT |
| Evidence honesty | good on what it measures | strong, generated-from-CSV | strong + **published retractions** | failures kept in CSVs | inputs missing from repo | self-critical + one bad "match" row (`e226` rel.err 3.8e-01) | **stale README vs own CSVs** | self-undercutting | **R16 fabricated** |

Legend: ✅ implemented & evidenced · ⚠️ partial/unproven · ❌ absent. Our column cites our own
audit ([[09-research-code-alignment]] §6.1, [[FINAL-AUDIT-REPORT]] §01).

## 19.2 R1–R20 check — us vs the four strongest rivals

Only cells where a rival differs meaningfully from us are expanded; full R1–R20 for us is
[[09-research-code-alignment]] §6.1 (4 GOOD / 10 PARTIAL / 3 GAP / 1 REGRESSION / 2 UNPROVEN).

| Req | markov-cero | SANKHYA (thegoodeng.) | team-vertexx | refinery-opt | VX03 |
|---|---|---|---|---|---|
| R4 IPM | **GAP** (papers only) | ✅ Mehrotra | ❌ | ✅ Mehrotra | ✅ HSD |
| R5 cuts in-tree | PARTIAL (root-only) | ✅ root GMI on + parallel MIP | ✅ in-tree set | ✅ root GMI | ✅ GMI/cover |
| R7 multicore | **REGRESSION 0.56×** | ✅ measured | ✅ pool | ✅ pool | ⚠️ |
| R8 GPU measurable | UNPROVEN (loses 13/13) | ✅ honest 1.4–6.5× | ✅ T4 | ⚠️ FP32 | ✅ |
| R16 comparison | **GAP (hard)** | ✅ strongest | ✅ in CI | ✅ win claimed (thin) | ✅ in CI |
| R17 robustness demo | **GAP** | ✅ independent verifier + exact-rational | ⚠️ retractions but no dossier | ❌ | ⚠️ 19/22 |
| R10 from scratch | ✅ CI guard | ✅ ldd gate | ✅ | ✅ scanner | ⚠️ deps list |
| R14 API/CLI | ✅ 3 CLIs, no install() | ✅ CLI+bindings | ✅ | ✅ | ✅ py |

**Reading:** the two hard PS gaps the audit flagged for us (R4, R16) are closed by at least
four rivals, and R7/R8 are honestly closed by SANKHYA. Our only outright lead is *breadth of
trust machinery* (independent verifiers + provenance + 43 ctest targets + a 204-reference
research corpus) — and SANKHYA's ldd provenance gate means that lead is narrower than we
assumed ([[18-risk-register]] LIC/SCP rows deserve an update after this note).

## 19.3 Established-solver reference column

What "comparison-grade" means when the evaluator (or we) measures against incumbents:

| Solver | LP engines | MILP depth | QP | GPU | Licence | Standing |
|---|---|---|---|---|---|---|
| **HiGHS** | simplex pr+du, IPX/HiPO IPM **+ crossover**, HiPDLP | branch-and-cut, strong presolve, multithreaded since 2026 | convex QP, no MIQP | cuPDLP-C + HiPDLP (v1.10+) | MIT | best open source: LPfeas 17.2 / MILP 7.36 vs COPT 1.00 [Observed: Mittelmann] |
| **SCIP** | SoPlex simplex (exact rational) | deepest: branch-cut-and-price, plugins | via nonlinear handlers | no | Apache-2.0 (since 8.0.3) | MILP 9.93 [Observed] |
| **CBC/CLP** | simplex + basic IPM | rich CGL cuts (MIR, cover, clique…) | no native | no | EPL-2.0 | order behind HiGHS [Observed] |
| **GLPK** | simplex | weak cuts/heuristics | no | no | **GPL-3.0 copyleft** | bottom tier |
| CPLEX/Gurobi/Xpress | all three engines | production depth | full MIQP | Gurobi/Xpress PDLP | commercial | **withdrawn from Mittelmann** (2024) [Observed] |

- **Honest student-solver bar** [Observed: mipx]: a from-scratch C++23 B&C solver measures
  itself at 2–5× slower than HiGHS on medium Netlib, 10–40× on the hardest — which is exactly
  the band our rivals publish (SANKHYA 2.62×, Deekshith 2.57×, team-vertexx 62%, VX03 13–350×).
  **Being 2–10× slower than HiGHS while publishing it is the market position**, not a weakness.
- **What we can claim that they cannot**: permissive licence + zero third-party deps + auditable
  math-to-code traceability ([[21-traceability]], 204-paper corpus) + Indian refinery-domain
  models ([[09-research-code-alignment]] R11). Not speed.

## 19.4 Prior-art column (non-SIH)

- **GPU LP:** cuPDLP-C (C rewrite, sits inside COPT 7.1), cuPDLP.jl, cuPDLPx (Halpern+PID,
  2.5–6.8× over cuPDLP), **NVIDIA cuOpt** (concurrent GPU PDLP + barrier; its MILP still says
  "proving optimality remains under active development" [Observed: README]). Mittelmann now has
  a dedicated GPU LPfeas column. → the GPU-claim floor keeps rising; nobody can win "fastest GPU".
- **QP:** cuOSQP (CUDA ADMM, QP-only) — our ADMM QP is a from-scratch re-derivation, cite it
  as prior art, never as competition.
- **Indian ecosystem:** IIT Bombay **Minotaur** (MINLP, with Argonne), FOSSEE Scilab toolbox
  (wraps COIN-OR — not indigenous core), a 2026 NIET patent on GPU-ADMM/PDHG (adjacent);
  **no DRDO/government indigenous solver initiative found** [Not found]. The sovereignty gap
  is real: nobody owns "Indian solver core" publicly.
- **From-scratch precedent:** mipx (published HiGHS-relative scores), JAG954/optimization,
  CHOP — none Indian, none SIH.

## 19.5 Gaps nobody in the field covers

1. **R4+R7+R8 together** — no single competitor has IPM+crossover **and** measured multicore
   scaling **and** an honest GPU story in one artifact: SANKHYA has all three but hasn't run
   its GPU-vs-OR-Tools gate ("Not yet run" `BENCHMARKS.md:461`); refinery has IPM+threads but
   no CI; IGAOS has GPU but no IPM/parallel at all. *This is the combination our roadmap's P1
   (IPM) + P0-4 (parallel) would occupy — if built and evidenced.*
2. **Robustness dossier (R17)** — SANKHYA has independent verification; nobody ships a curated
   degeneracy/ill-conditioning/weak-relaxation dossier as a *named deliverable*. Our P0-5 can
   be first if it lands by 2026-09-30.
3. **Reproducible-from-clone benchmarks** — refinery's `data/*` dirs are empty; IGAOS's
   hardware sheets are gitignored; Igaos-public has no CI. A one-command `run_compare.py` that
   re-downloads and re-runs beats all of them on evaluator trust.
4. **Licence clarity** — four T1 rivals ship with no LICENSE file, a non-OSI licence, or an
   unfilled `<TEAM NAME>` placeholder. We ship Apache-2.0 + NOTICE + PROVENANCE.
5. **Mittelmann/QPLIB breadth** — nobody (including us) has Mittelmann instances committed;
   first to a Mittelmann column wins R15/R20 breadth.

## 19.6 Claims we must **not** make

1. ❌ "Faster than HiGHS/Gurobi" — every honest rival publishes 2–350× *slower*; we have zero
   baseline. Even a true micro-win reads as cherry-picking without the harness.
2. ❌ "GPU-accelerated" unqualified — our own crossover study loses 13/13 and `run_gpu.py`
   hides a BLEND failure ([[00-ground-truth]] §C). SANKHYA's honest 1.4–6.5× is the bar.
3. ❌ "Parallel speedup" — 0.56× measured; rivals publish scaling curves.
4. ❌ "Benchmarked on MIPLIB/Netlib/Mittelmann vs established solvers" — true only for
   Netlib/MIPLIB thin sets, and R16 is empty. Vendors of this exact sentence with no artifacts
   exist (Firefly, bharatopt) — association risk.
5. ❌ "Compared with CPLEX/Gurobi" — only Igaos-public does (bench-only imports); we never run
   commercial solvers and never will.
6. ❌ Anything softer than "measured on <hardware> at <commit>" — SANKHYA stamps instance
   sha256 + git commit + machine tag in every CSV [Observed]. That is now the expected format.

## 19.7 Threat ranking (why each row sits where it does)

| # | Project | Threat | Single reason |
|---|---|---|---|
| 1 | **SANKHYA** (thegoodengineers + Deekshith2205 snapshot) | **Very high** | Only rival closing R4+R5+R7+R8+R16+R10 *simultaneously*, with CI that fails on a linked solver lib (`ci.yml:814`) — our provenance differentiator, duplicated. |
| 2 | **team-vertexx/sankhya** | High | R16 runs **inside CI** (every Netlib push vs published optima) and publishes retractions — the trust story an evaluator can verify themselves. |
| 3 | **VioniX37/VX03** | High | Polished HSD-IPM + PyTorch GPU + CI benchmark job + self-critical numbers; soft spots are licence (PolyForm) and deps hygiene. |
| 4 | **trijalpgunaseelan/Igaos-public** | High | Broadest engine set of any student repo (IPM, MIQP, NLP, IIS) with CPLEX 13/13 + Gurobi 21/22 CSVs; no CI, ambiguous provenance (squashed commit, unfilled licence). |
| 5 | **kavinR-11/refinery-optimizer** | Medium-high | The only published HiGHS *win* (MIPLIB 80%, 5 inst) plus real Mehrotra + thread pool + GPU; no CI, missing benchmark inputs, no LICENSE. |
| 6 | **Lothnic/IGAOS** | Medium-high (packaging), medium (solver) | PyPI wheels + deck ledger out-package everyone; but serial MILP at 4/240, simplex 10–100× slower than HiGHS, and README/PyPI headline numbers that **understate their own committed evidence** — exploitable as inconsistency, not dishonesty. |
| 7 | **Satyanshgaur/PIPEPYE** | Medium | Genuine quantified R16 they mostly lose (18/32) + CUDA CI; scores honesty, not capability. |
| 8 | **RaghavGupta2910/optimisation_solver** | Medium | 25.8k clean MIT C++ with tests and 22 MPS fixtures — but no results files, no CI, no IPM/GPU. |
| 9 | **akshayvarma121/Firefly_solver** | Medium (demo flash), low (evidence) | Real CUDA LP+MILP behind a slick web UI, but R16 is two hardcoded numbers, no CI, 98% of tracked files are committed build output. |
| 10 | T2 rest (bharatopt, sovereign-solver, Kouhsik33, mohitsaitummalapalli, nomos, ApexCUDA pair, hemasri) | Low | Partial engines, fabricated or absent evidence, broken repos. |
| 11 | T3 (10 repos) | None | Pitches, PS scrapes, zero-byte scaffolds. |

## 19.8 Counter-moves (report-only analysis — no roadmap edits here)

These are *findings about the field*, deliberately kept out of [[15-roadmap]] per scope; the
roadmap owners decide what to adopt.

1. **R16 is now existential, not just a rubric line.** Seven rivals publish HiGHS (or
   CPLEX/Gurobi) comparisons; two run them in CI. Our audit's P0-1 (`scripts/run_compare.py`)
   remains the single highest-leverage action — it is the difference between "no baseline in a
   field where everyone has one" and competing on the same axis. One day of work; do it first
   ([[13-restart-point]] step 1 unchanged).
2. **Adopt SANKHYA's evidence format, then beat its bar.** Generate `evidence/compare/report.md`
   *from* a committed CSV with instance sha256 + commit + machine tag (their discipline), and
   quote the rivals' honest numbers as the ladder: IGAOS 100/114, SANKHYA 81/89 @1e-6,
   team-vertexx 82/88, VX03 23/23. Ours must be per-instance or it won't be believed.
3. **The IPM fork gains urgency but not certainty.** Four rivals have IPM (R4 explicitly named
   in the PS). If P1-IPM can't land with evidence by the deadline, the "evidence-backed
   re-scope" branch must say so *explicitly in the demo*, because an evaluator comparing repos
   will see `src/ipm/` in theirs and papers in ours.
4. **R7/R8: fix or mute, because SANKHYA did both honestly.** Their unreleased GPU gate is our
   timing window: if our GPU loses 13/13 *after* the `run_gpu.py:241-245` status fix, scope the
   claim to CPU-first-order (per [[ED-007-honest-gpu-scoping]]); a wrong claim now stands out
   more because the field is publishing honest losses.
5. **Provenance is contested ground.** Our "CI-guarded sovereignty" is duplicated by
   SANKHYA/Deekshith's ldd fail-on-solver gate and refinery's scanner. Keep the guard, but
   lead with what nobody has: 204-reference research→code traceability ([[21-traceability]]) +
   independent zero-trust verifiers + refinery-domain case models (R11).
6. **Do not attack IGAOS's honesty — cite their CSVs.** Their seam is README/PyPI staleness
   (52/64 vs committed 63/64; 6/20 vs 8–10/20; 10/15 vs 12/15). Positioning: "we publish
   numbers that our own artifacts back, generated from CSV" — their own best artifacts
   support a bar *we can quote without venom*.
7. **Name distinctiveness is real.** With 3×SANKHYA, 2×IGAOS, 3×BharatOpt, 2×ApexCUDA in the
   field, keep `markov-cero` + Apache-2.0 + `PROVENANCE.md` visible in README, deck, and video;
   evaluator confusion is a risk to them, a moat for us ([[17-sih-demo-strategy]] slide 1).

## 19.9 Sources

- Local clones: `/tmp/opencode/competitors/<repo>` (27 repos, `--depth 1`, 2026-09-25).
- Per-repo deep notes with file:line citations: [[Competitive Landscape MOC]].
- Established solvers: highs.dev, scipopt.org, coin-or, Mittelmann tables (plato.asu.edu),
  mipx repo. Prior art: cuOSQP, cuPDLP-C/-x, NVIDIA cuOpt, IIT Bombay Minotaur, FOSSEE.
- Our standing: [[FINAL-AUDIT-REPORT]] §01, [[09-research-code-alignment]] §6.1,
  [[00-ground-truth]] §C.

## Referenced By

- [[SIH Strategy MOC|audit/SIH Strategy MOC]]
- [[Bharat-Opt (Kouhsik33)|competitive/Bharat-Opt (Kouhsik33)]]
- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]
- [[Established solvers|competitive/Established solvers]]
- [[Firefly solver (akshayvarma121)|competitive/Firefly solver (akshayvarma121)]]
- [[IGAOS (Lothnic)|competitive/IGAOS (Lothnic)]]
- [[Igaos-public (trijalpgunaseelan)|competitive/Igaos-public (trijalpgunaseelan)]]
- [[Mid-tier solver repos|competitive/Mid-tier solver repos]]
- [[PIPEPYE (Satyanshgaur)|competitive/PIPEPYE (Satyanshgaur)]]
- [[Placeholder and empty repos|competitive/Placeholder and empty repos]]
- [[Prior art - solver and GPU projects|competitive/Prior art - solver and GPU projects]]
- [[SANKHYA (thegoodengineers)|competitive/SANKHYA (thegoodengineers)]]
- [[VX03 (VioniX37)|competitive/VX03 (VioniX37)]]
- [[bharatopt (JosephXpanakaL)|competitive/bharatopt (JosephXpanakaL)]]
- [[optimisation solver (RaghavGupta2910)|competitive/optimisation solver (RaghavGupta2910)]]
- [[refinery-optimizer (kavinR-11)|competitive/refinery-optimizer (kavinR-11)]]
- [[sankhya (team-vertexx)|competitive/sankhya (team-vertexx)]]
- [[sovereign-solver (AryanMotiani)|competitive/sovereign-solver (AryanMotiani)]]