---
type: moc
tags: [moc, sih, audit, submission, strategy]
status: complete
date: 2026-09-25
---

# SIH Strategy MOC — submission navigation hub

> One entry point for everything an evaluator (or a teammate joining on 2026-09-26) needs to read
> in order: what was asked, what is true today, what we were graded on, what we will show, what we
> will build first, what could go wrong, and how each claim is traced back to evidence.
> Deadline: **idea submission 2026-09-30** (constraint C5 in [[00-ground-truth]]).

## Start here (reading order)

1. [[sih26119_problem_statement|SIH26119 Problem Statement]] — the verbatim brief: R1–R20 checklist, constraints C1–C6, evaluation criteria.
2. [[00-ground-truth|Ground Truth]] — the four sources of truth [A]–[D], the divergence table, PS-GAP-01…06. Read before trusting any claim in `docs/`.
3. [[10-sih-evaluator-report|SIH Evaluator Report]] — simulated strict scoring (§7.1–7.9) and today's verdict **B− → A− if P0 closes**.
4. [[FINAL-AUDIT-REPORT|Final Audit Report]] — the consolidated 22-section report plus the lead technical architect's answer.
5. [[13-restart-point|Restart Point]] — where work resumes: the evaluation layer, not the core; step 1 is `scripts/run_compare.py`.

## Build the thing

6. [[07-current-architecture|Current Architecture]] — as-built pipeline, component dependency map, and the **AP-1…AP-12** problem table.
7. [[08-codebase-audit|Codebase Audit]] — directory and module verdicts (KEEP / MODIFY / REWRITE / REMOVE), doc-vs-code discrepancies.
8. [[09-research-code-alignment|Research ↔ Code Alignment]] — R1–R20 observed status, the gap list, missing experiments E1–E6.
9. [[12-keep-remove-rebuild|KEEP / REMOVE / REBUILD]] — RW-1…RW-10 rebuild rows and the explicit "keep the core, rebuild the edges" verdict.
10. [[14-target-architecture|Target Architecture]] — current → problems → target diagram and the nine-row diff table.

## Prove it, ship it, defend it

11. [[16-testing-evaluation-strategy|Testing & Evaluation Strategy]] — test additions, metric definitions, CI changes, requirement→proof DoD.
12. [[17-sih-demo-strategy|SIH Demo Strategy]] — beat-by-beat 5–10 min script, 6-slide deck outline, non-AI video rules, fallbacks.
13. [[15-roadmap|Implementation Roadmap]] — P0–P3 priorities, dependency graph, the 5-day schedule sketch.
14. [[18-risk-register|Risk Register]] — K1–K7 plus SCH/IPM/EVD/GPU/LIC/BUS/SCP rows, detectors, and the top-5 before Sep 30.
15. [[21-traceability|Research → Code Traceability]] — the per-requirement research/code/delta/verification matrix and the P0 closure register.
16. [[Research-Code Traceability MOC]] — the navigation layer *around* that matrix, with worked example walks in both directions.
17. [[19-competitive-landscape|Competitive Landscape]] — full-scale rival survey (27 repos, 9 real competitors): threat ranking, where the field stands on R4/R7/R16, claims we must not make, report-only counter-moves. Vault: [[Competitive Landscape MOC]].

## 5-day critical path (P0, from [[15-roadmap]])

All six must be evidenced before 2026-09-30; the definition of done per item is in [[15-roadmap]] §P0, and the order they are executed in is [[13-restart-point]] steps 0–7.

| # | P0 item | Objective | Artifact it must produce |
|---|---|---|---|
| P0-1 | **Comparison harness (R16)** — the single most important item | honest head-to-head vs ≥1 established solver on a shared instance set | `scripts/run_compare.py`, `evidence/compare/{results.csv,profile.png,report.md}` |
| P0-2 | **Claims audit + hardware manifest + GPU status fix (R8/R7/R18)** | make every public claim match the evidence | `evidence/hardware.md`, fixed `scripts/run_gpu.py:241-245`, claims↔evidence table |
| P0-3 | **In-tree cut loop (R5, RW-1)** | cuts re-separated inside branch-and-bound; node reduction > 0 | `evidence/cut_effectiveness.csv` (≥20 % on ≥2 of 3 MIPLIB instances) |
| P0-4 | **Parallel fix or demotion (R7, RW-2)** | stop shipping a measured 0.56× regression | work stealing + scaling curve, *or* `--threads 1` default and the claim deleted |
| P0-5 | **Robustness dossier (R17/R13, E3)** | the numerical-robustness demonstration the PS names | `data/robustness/*`, `scripts/run_robustness.py`, `evidence/robustness/report.md` |
| P0-6 | **Demo packaging (SIH rules C6)** | 6-slide PDF, narrated non-AI video, working demo script | `run-qualification-demo.sh` v2, `reports/demo/*`, deck + video |

Per-person assignment lives in [[15-roadmap]] §Schedule sketch; execution order of the top risks lives in [[18-risk-register]] §18.3; the fallback if any P0 slips is in [[21-traceability]] §21.2.

## Guardrails (constraints that shape every P0)

- **C1 — from scratch, no OSS solver library.** Comparing against HiGHS in P0-1 is *not* a violation: running an external solver as an oracle is what R16 demands; `scripts/check-sovereignty.py` forbids linking/vendor-sourcing only ([[00-ground-truth]] §A.6).
- **C3 — API or CLI only, no GUI.** Nothing in the P0 list builds interface work; R14 is judged on a thin CLI + an `install()`-able library (RW-4, P1).
- **C4 — GPU only where it measurably pays.** P0-2 measures first; if the data says CPU wins, the claim is narrowed, not defended ([[ED-007-honest-gpu-scoping]]).
- **C5 — idea submission 2026-09-30.** Everything P1+ is explicitly deferred past the deadline ([[18-risk-register]] §18.3 "accepted risk").
- **C6 — SIH process:** 6-slide PDF deck, non-AI demo video with live team narration, public GitHub partial implementation ([[17-sih-demo-strategy]] §17.5 for the video affidavit).
- **Rule 11 — simplest architecture that satisfies the brief:** no GUI, no cloud, no ML branching, no rewrite of passing cores ([[14-target-architecture]] non-goals).

## Three questions this hub answers

1. **"What must be true to submit?"** → P0-1…P0-6 above, gated by K1 / K2 / K6 in [[18-risk-register]].
2. **"What does the evaluator already see?"** → [[10-sih-evaluator-report]] scores, over the honest gaps in [[09-research-code-alignment]] §6.1.
3. **"Where is any claim proved?"** → [[21-traceability]] §21.1 → component note → `evidence/`, indexed by [[04-evidence-inventory]].

## Related hubs

[[Research MOC]] · [[Codebase MOC]] · [[Architecture MOC]] · [[Algorithms MOC]] · [[Datasets MOC]] · [[Evaluation MOC]] · [[Technical Debt MOC]]
