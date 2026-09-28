---
type: moc
tags: [moc, datasets, benchmarks, research]
status: complete
date: 2026-09-25
---

# Datasets MOC — benchmark data in the vault and the repo

> The four dataset notes the research layer defines, exactly where each one lives in this
> repository, and which audit documents grade the gap. Counts below were verified with `ls` on
> 2026-09-25. Dataset requirements come from R15/R19 of
> [[sih26119_problem_statement|SIH26119 Problem Statement]].

## 1. The four dataset notes

| Note | PS requirement | Present in repo? | Repo location | Coverage today |
|---|---|---|---|---|
| [[Netlib LP Collection]] | R15, R19 | **yes** | `data/netlib/` | 17 MPS files; 7 run in `netlib_benchmarks`, 5 more in the extended run (`evidence/netlib_results.csv`, `evidence/netlib_extended.csv`) |
| [[MIPLIB]] | R15, R19 | **yes (thin)** | `data/miplib/` | 3 MPS files (`stein9`, `stein15`, `flugpl`); `evidence/miplib_results.csv` |
| [[Mittelmann Benchmarks]] | R15 | **no** | — (PS-GAP-04) | zero instances; no `data/mittelmann/` directory exists |
| [[QPLIB]] | R19 (R2 makes QP in scope) | **no — directory exists but is EMPTY** | `data/qp/` | 0 bytes; no QP instance has ever been run — the QP engine [[QP-ADMM-Engine]] is exercised only on `examples/qp_portfolio.mps` |

## 2. Other data that is not one of the four named libraries

| Location | What it is | Verified state | Graded by |
|---|---|---|---|
| `data/scale_study/` | synthetic CSC-scale instances `scale_5.mps` … `scale_50000.mps` | 14 files present locally, **gitignored** (`.gitignore`), so not reproducible from a clone | [[08-codebase-audit]] §4.1, [[16-testing-evaluation-strategy]] (R12 scale proof) |
| `examples/refinery/` | the R11 industrial case study: feasible / infeasible / limited / malformed MPS + `expected-results.json`, `data-dictionary.md`, `README.md` | present and wired into CLI tests; this is the only "case study" claim the repo can make (R11, R19) | [[08-codebase-audit]] §4.1 · [[16-testing-evaluation-strategy]] §16.2 |
| `examples/blend.mps`, `examples/qp_portfolio.mps` | demo/CLI-test models | present; `blend` is also the instance that fails on GPU ([[blend-numerical-failure]]) | [[07-current-architecture]] §C.1 |
| `evidence/` + `reports/` | measured *results* over the data above (not instances) | no hardware metadata anywhere; `reports/` gitignored | [[04-evidence-inventory]] |

## 3. Where each dataset is graded

- **[[08-codebase-audit]]** — §4.1 row-by-row inventory: `data/netlib` KEEP, `data/miplib` MODIFY (expand), `data/qp` MODIFY (add data or delete), `data/scale_study` KEEP (verify git status), `examples/` KEEP as demo assets.
- **[[16-testing-evaluation-strategy]]** — §16.2 benchmark layer: which ctest target proves which suite (`netlib_benchmarks`, `miplib_benchmarks`, new `mittelmann_benchmarks`, new `qplib_benchmarks`); §16.3 tests 13 (Mittelmann smoke) and §16.6 DoD rows for R15/R19.
- **[[09-research-code-alignment]]** — §6.1 R15 (2 of 3 named suites used, Mittelmann absent) and R19 (2 of 4 named sets ingested).
- **[[21-traceability]]** — §21.1 rows R15 and R19 carry the research evidence, current-code observation and verification artifact per dataset.
- **[[04-evidence-inventory]]** — per-file evidence table: instance counts and the R numbers each artifact supports.
- **This note feeds [[Evaluation MOC]]** — a suite is only a benchmark once a metric and a baseline are attached.

## 4. Dataset gaps → actions

| Gap | Evidence | Action (priority) |
|---|---|---|
| Mittelmann set absent | [[Mittelmann Coverage Gap]]; `ls data/` → no `mittelmann/` | P1-6: add LP + MILP sets, `data/mittelmann/`, new runner ([[15-roadmap]]) |
| `data/qp/` empty | `ls data/qp` → 0 files | P1-6 / P2-1: QPLIB-style subset or delete the directory ([[12-keep-remove-rebuild]] §8.2) |
| MIPLIB breadth = 3 instances | [[MIPLIB]] note; `evidence/miplib_results.csv` | expand to the benchmark subset before any MILP performance claim |
| Scale instances not in git | `data/scale_study` gitignored | vendor a small scale set so R12 evidence reproduces from a clone (P1-5) |
| No instance *provenance* recorded | [[04-evidence-inventory]] (no hardware, no checksums) | hardware manifest + checksums with the P0 harness ([[13-restart-point]] step 0) |

## 5. Reading paths

- **Evaluator asking "which benchmarks?"** → [[Netlib LP Collection]] · [[MIPLIB]] · [[Mittelmann Benchmarks]] · [[QPLIB]] → [[16-testing-evaluation-strategy]] §16.6 DoD.
- **Engineer asking "what do I run on?"** → `data/` table above → [[benchmark-suites]] → [[Evaluation MOC]] experiments E1–E6.
- **Auditor asking "is the data claim true?"** → [[08-codebase-audit]] §4.1 → [[04-evidence-inventory]] → [[00-ground-truth]] §C.3.
- **Designer asking "what data does the target need?"** → [[14-target-architecture]] evaluation layer → [[15-roadmap]] P1-6 (Mittelmann + `data/qp` non-empty with ≥5 instances).

## 6. Ingestion, licensing, reproducibility

- **Netlib** — free/public, no access barrier; `scripts/run_netlib.py:193` re-downloads on demand, so a clone *without* network still has the 17 vendored MPS files.
- **MIPLIB** — community library; only 3 instances are vendored, so any MILP performance claim today rests on a very thin slice ([[MIPLIB]] notes the benchmark-subset rule).
- **Mittelmann** — a *leaderboard*, not a downloadable set: adopting it means fetching the named LP/MILP sets and reporting with Mittelmann's aggregation style ([[Mittelmann Benchmarks]], [[Geometric Mean Runtime]]).
- **QPLIB** — standard format + checker; convex results must be reported separately from non-convex handling behaviour ([[QPLIB]]).
- **Offline risk:** a blocked download kills the benchmark and comparison beats — that is risk **LIC-01** in [[18-risk-register]] (vendor a licence-clean `data/compare/` set with checksums).
- **Reproducibility rule:** every dataset run must land in `evidence/` with instance count, hardware id and checksum; today none of the three columns exist ([[missing-hardware-metadata-in-evidence]]).

## Related

[[Architecture MOC]] · [[Algorithms MOC]] · [[Evaluation MOC]] · [[Research MOC]] · [[Codebase MOC]] · [[09-research-code-alignment]] · [[15-roadmap]]
