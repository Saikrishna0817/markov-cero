---
type: audit
phase: 13
title: Final Audit Report — markov-cero / SIH26119
tags: [audit, final-report, phase-13]
status: complete
date: 2026-09-25
---

# Final Audit Report — markov-cero (SIH26119)

> Historical report. Its unqualified clean-room wording is superseded by the source-exposure
> disclosure in `PROVENANCE.md`; clean-room status remains unverified pending independent review.

> Four sources of truth: **[A]** PS (`docs/sih26119_problem_statement.md`) ·
> **[B]** research (`docs/research/`) · **[C]** code (`docs/codebase/`, `docs/audit/07-09`) ·
> **[D]** target (`docs/audit/14-15`). Labels: *Observed fact* / *Research finding* /
> *Inference* / *Recommendation*.

## 01. Executive Summary

markov-cero is a **clean-room C++20 LP/MILP/QP/MIQP solver core** (~21k LOC incl. GPU/tests)
with enforced sovereignty (CI guard), independent zero-trust verifiers, and 43 CTest targets —
substantial, honest engineering. Against PS SIH26119 it scores **4 GOOD / 10 PARTIAL /
3 GAP / 1 REGRESSION / 2 UNPROVEN** across R1–R20. The three evaluator-facing failures:
**(1) zero comparison against any established solver (R16 — explicit PS requirement),
(2) no interior-point method (R4 — explicitly named), (3) two self-contradicted claims**
(parallel 0.56× speedup; GPU loses 13/13 measured, plus a benchmark bug hiding a numerical
failure). The research corpus (204 references) was collected but never metabolized into the
code. **Verdict: B− today, A− achievable within the 5-day window** by closing six P0 items —
starting with the comparison harness, not new algorithms.

## 02. Original Problem Statement

Verbatim record recovered from the official portal (it was *absent from the repo* — PS-GAP-01,
fixed). Summary: sovereign **solver core**, from scratch, LP/MILP/QP first (MIQP/NLP/MINLP
later), revised simplex **and interior-point**, B&B/B&C/cuts/presolve/heuristics, sparse +
multicore + GPU-if-beneficial, thousands→millions scale, robustness on degeneracy/
ill-conditioning, API/CLI (no GUI), benchmarks on MIPLIB/Netlib/Mittelmann, **comparison vs
≥1 established solver**, robustness demonstration. Full requirement table R1–R20:
`docs/sih26119_problem_statement.md`; distilled ground truth: `docs/audit/00-ground-truth.md`.

## 03. Current Project State

v0.5.2, "Phase 6 complete"; git history shows Phases 1–6 (clean-room → simplex → MILP →
PDLP → GPU → QP). Architecture: MPS → canonicalize (dense gate|sparse) → 3-rule presolve +
Ruiz → 8-engine dispatch inside a 499-line CLI → postsolve → verifiers → JSON.
Evidence: Netlib 7 instances optimal (rel err ≤7.9e-15) + 5 extended; MIPLIB 3;
crossover study (GPU loses 13/13); `phase4.json` (parallel 0.56×, cuts 0.0%).
Full state: `docs/audit/00-ground-truth.md` §C, `docs/audit/07-current-architecture.md`.

## 04. Research Landscape

204 references, 17 modules (survey → sparse LA → simplex → IPM → presolve → QP → MILP →
cuts → heuristics → branching → numerics → parallel/GPU → benchmarking → domain → MIQP/MINLP →
ML), now materialized as **202 paper notes** + 72 concept/algorithm/technique/dataset/metric/
architecture/limitation/gap notes + 10 engineering decisions + 2 synthesis maps.
Index: `docs/research_paper_references.md`; graph entry: [[Research MOC]].

## 05. Research Knowledge Synthesis

`docs/research/maps/cross-paper-synthesis.md` (repeated techniques, contradictions — e.g.
simplex-degeneracy camp vs first-order-GPU camp, complementary pairs — IPM+crossover+simplex,
common benchmarks/metrics, ideas NOT to combine — ML branching in a 5-day window) and
`docs/research/maps/research-dependency-map.md` (9-layer dependency chains with a
"current code" status line per layer). The collective verdict: a credible solver needs
sparsity-first foundations, dual simplex with steepest-edge for node work, an IPM+crossover
pair, deep presolve, in-tree cuts, load-balanced parallelism, and a Dolan–Moré evaluation
discipline.

## 06. Research Gaps

Not implemented but research-justified: IPM+crossover · steepest-edge dual pricing ·
in-tree cut regeneration + pool · work stealing · iterative refinement + condition estimation ·
Markowitz/AMD ordering · performance profiles + geometric means · presolve depth ·
diving/RINS · cover/odd-hole cuts. Research judged *unsuitable now*: ML branching, NLP/MINLP
engines, PGAS/asynchronous MIP, dense-era IPM. Code with **no** research/PS justification:
duplicate runners, dead options, dense-gate threshold, the "29320×" claim. Detail:
`docs/audit/09-research-code-alignment.md` §6.2–6.4.

## 07. Current Architecture

Verified pipeline diagram, component dependency map, and 12 architectural problems (AP-1..12):
`docs/audit/07-current-architecture.md`. Architecturally right: clean-room CI, verifiers,
CSC+LIFO-presolve, multi-engine dispatch. Architecturally weak: no IPM, no evaluation layer,
root-only cuts, scheduler without balancing, orchestration trapped in the app, dual
canonicalization paths.

## 08. Codebase Audit

Full directory + module inventory with per-group verdicts: `docs/audit/08-codebase-audit.md`
— **KEEP 18 · MODIFY 17 · REWRITE 5 · REMOVE 3 · INVESTIGATE 1 pair**; hidden-functionality
check before removals; doc-vs-code discrepancy table; security posture (no secrets, parser =
attack surface, fuzz target unwired).

## 09. Research ↔ Code Alignment

The R1–R20 matrix with research sources, expected vs current implementation, status and
action: `docs/audit/09-research-code-alignment.md` (+ missing experiments E1–E6, unsuitable
research, areas where the code *exceeds* research baseline).

## 10. SIH Evaluator Assessment

`docs/audit/10-sih-evaluator-report.md` — alignment 6.5/10, innovation 5→7/10 reframed,
depth 7/10, evaluation 3/10; what works vs what fails live; the honest 4-minute demo today
vs the 8-minute award demo needed; top risks K1–K7. **Grade: B− today, A− after P0.**

## 11. Critical Problems

1. **R16 gap** — no external-solver comparison exists anywhere (fatal-ish).
2. **R4 gap** — no interior-point method (PS names it).
3. **Credibility** — own evidence contradicts own claims (parallel 0.56×, GPU 13/13 losses,
   `run_gpu.py` status bug hiding `BLEND` failure).
4. **R17 gap** — robustness tests exist; robustness *demonstration* doesn't.
5. **R7 regression** — parallel search is negative value as shipped.
6. **Schedule** — Sep-30 deadline, 5 days, plus deck/video.

## 12. KEEP / REMOVE / REBUILD

`docs/audit/12-keep-remove-rebuild.md` — KEEP (core engines, verifiers, provenance, QP,
examples, evidence), REMOVE (dup runners, dead options, stale scratch dirs, false claims),
REBUILD RW-1..RW-10 (cut loop, parallel scheduler, comparison harness, API extraction,
sparse-first canonicalization, presolve depth, steepest-edge, numerics tooling, GPU
evaluation, docs layer). Explicit verdict: **do not restart the core; rebuild the edges;
replace the evaluation layer outright.**

## 13. Restart Point

`docs/audit/13-restart-point.md` — freeze `v0.5.2-audit-baseline` + hardware manifest →
**build `scripts/run_compare.py` (comparison harness) first** → claims audit → in-tree cuts →
parallel fix-or-demote → robustness dossier → IPM decision → demo packaging. Why: it is the
only 100%-missing hard requirement, it gates credibility, it is the measuring instrument for
every other P0 fix, and it is unblocked by existing assets.

## 14. Target Architecture

CURRENT → PROBLEMS → TARGET with full diagram: `docs/audit/14-target-architecture.md` —
same sound core, plus in-tree cuts, steepest-edge, IPM+crossover, load-balanced scheduler,
numerics tools, library API, and a first-class **evaluation layer** (comparison harness,
suites, dossier, hardware manifest). Non-goals: no GUI/web/DB, no ML this cycle.

## 15. Implementation Roadmap

P0–P3 with files, research basis, tests, DoD, risks, dependency graph and a 3-person × 5-day
schedule: `docs/audit/15-roadmap.md`. P0 (6 items): comparison harness · claims audit + bug
fix · in-tree cuts · parallel fix/demote · robustness dossier · demo packaging.

## 16. Testing & Evaluation Strategy

`docs/audit/16-testing-evaluation-strategy.md` — per-layer strategy, ≥10 named new tests,
metric definitions, CI changes, requirement→proof DoD table.

## 17. SIH Demo Strategy

`docs/audit/17-sih-demo-strategy.md` — timed demo script, story arc (sovereignty →
certificates → comparison → robustness → honest GPU), 6-slide deck outline, non-AI video
plan, per-failure fallbacks.

## 18. Risk Register

`docs/audit/18-risk-register.md` — 14 risks with evidence, likelihood/impact/detectability,
mitigations, top-5 before Sep 30.

## 19. Research Knowledge Graph

340+ notes total; research side: `docs/research/` — papers/ (202), concepts/ (16),
algorithms/ (17), techniques/ (13), datasets/ (4), metrics/ (8), architectures/ (3),
limitations/ (6), research-gaps/ (7), engineering-decisions/ (10), maps/ (2),
Research MOC. Entry: [[Research MOC]]. Backlinks maintained by `scripts/link_backlinks.py`.

## 20. Codebase Knowledge Graph

`docs/codebase/` — components/ (18), APIs/ (3), data-flow/ (1), tests/ (3),
technical-debt/ (11), bugs/ (2), decisions/ (5), audit/ (2), Codebase MOC.
Entry: [[Codebase MOC]].

## 21. Research → Code Traceability Matrix

`docs/audit/21-traceability.md` — R1–R20 rows linking PS clause → research notes → code
(file:line) → status → recommendation → verification artifact, plus finding→requirement→
component→metric table and both navigation directions. Bridge index:
[[Research-Code Traceability MOC]].

## 22. Final Recommended Development Sequence

Freeze baseline → hardware manifest → **comparison harness (P0-1)** → claims audit + GPU
status bug (P0-2) → in-tree cuts (P0-3) → parallel fix-or-demote (P0-4) → robustness
dossier (P0-5) → demo packaging (P0-6) → P1 (IPM+crossover, steepest-edge, presolve depth,
API, sparse-first, Mittelmann/QPLIB) → P2 (hygiene, heuristic depth, cut families, numerics
tooling, GPU CI, case studies) → P3 (ML branching, MINLP). Evidence in `15-roadmap.md`.

## Appendix A. Competitive landscape (added 2026-09-25)

`docs/audit/19-competitive-landscape.md` — full-scale SIH26119 rival survey: **27 GitHub
repos cloned and inspected at file:line level** (9 real competitor projects + 8 partial +
10 pitch/empty). Carries the R1–R20 field matrix, established-solver and prior-art reference
columns, the gaps nobody covers (R4+R7+R8 together; robustness dossier; licence clarity),
claims we must not make, the threat ranking (SANKHYA > team-vertexx > VX03 > Igaos-public),
and report-only counter-moves. Two findings tighten §01: **seven rivals already publish an
HiGHS comparison (two run it in CI)**, and **SANKHYA duplicates our provenance differentiator
with an ldd fail-on-solver-lib CI gate**. Per-repo vault notes: [[Competitive Landscape MOC]].

---

## The lead technical architect's answer

> **If I joined today, knowing everything above:**
>
> **KEEP** — the clean-room core and its trust machinery: sparse-basis simplex (primal+dual
> with Harris), PDLP/PDHG, the ADMM+LDLᵀ QP engine, the CSC model with LIFO presolve/
> postsolve, the independent verifiers, the MPS I/O, the sovereignty CI guard, the examples,
> and the benchmark runners. This is ~70% of a real solver and rewriting it would lose the
> deadline and the strongest differentiator (provable provenance + certified solutions).
>
> **THROW AWAY** — the evaluation layer as it stands (single-solver CSVs and every claim not
> backed by a baseline), the root-only cut wiring, the parallel scheduler's shared-heap
> design (measured 0.56×), the dense-gate canonicalization fork, the dead options and
> duplicate runners, and the claims in README/STATUS that our own evidence contradicts.
>
> **REBUILD** — cut management inside the tree (RW-1), parallel scheduling (RW-2), the app
> monolith into a real API (RW-4), presolve depth (RW-6), dual pricing with steepest-edge
> (RW-7), numerical robustness tooling (RW-8), GPU measurement (RW-9), docs layer (RW-10) —
> and add what research + PS demand but nothing exists for: **IPM + crossover (R4)**.
>
> **IMPLEMENT FIRST** — `scripts/run_compare.py`, the external-solver comparison harness.
> Reason: it is the only hard PS requirement with zero artifacts, it is pure addition (no
> core surgery, no sovereignty risk), it takes ~1 day, and it is the instrument without which
> every other fix (cuts, parallel, GPU, IPM) cannot be *proven*. Evidence: `09` §6.1 (R16),
> `10` §7.1, `13`, `15` P0-1. **Then** claims audit → cuts → parallel → robustness dossier →
> demo. IPM is the one strategic fork to decide on day 1 (implement vs evidence-backed
> re-scope) because it is the only P0/P1 item that may not fit in the window.
>
> Why not algorithms first? Because Phase 0–6 already demonstrated the failure mode:
> six phases of implementation ahead of evidence, ending with 0.0% cut-node reduction,
> 0.56× parallel speedup, and no baseline. The team does not need more code — it needs
> measurement, honesty, and three named PS boxes ticked.
