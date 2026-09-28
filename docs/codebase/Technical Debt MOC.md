---
type: moc
tags: [moc, codebase, technical-debt, bugs]
status: complete
date: 2026-09-25
---

# Technical Debt MOC — every open debt and bug note

> All 11 notes in `docs/codebase/technical-debt/` (verified with `ls`, 2026-09-25) grouped by the
> `severity:` frontmatter each note carries, plus the 2 notes in `docs/codebase/bugs/`. Debt is
> what the code costs us; the *verdict* on the code itself lives in
> [[12-keep-remove-rebuild]] (KEEP / REMOVE / REBUILD / RW-1…RW-10).

## Severity key

- **high** — evaluator-visible or contradicts a public claim; scheduled before 2026-09-30.
- **medium** — real cost, but a demo or a patch absorbs it.
- **low** — hygiene; clean up in one pass (P2-1 in [[15-roadmap]]).

## High severity (3)

| Note | One line | Proof | Owning action |
|---|---|---|---|
| [[no-external-baseline]] | nothing in `evidence/`, `reports/` or `benchmarks/` compares against HiGHS/CPLEX/Gurobi/CBC/SCIP → R16 has zero artifact | `rg -i "highs\|cplex\|gurobi\|cbc\|scip"` → 0 hits | **P0-1** harness ([[ED-001-comparison-harness-before-new-algorithms]]) |
| [[negative-parallel-scaling]] | 4 threads measure 0.56× speedup / 14 % efficiency — a *negative* result still claimed as multicore | `evidence/benchmarks/phase4.json:56-62` | **P0-4** fix or demote (RW-2, [[ED-006-load-balanced-parallel-or-demote]]) |
| [[missing-hardware-metadata-in-evidence]] | no evidence file records CPU/GPU/driver → every timing is non-reproducible | scan of `evidence/*.csv` and `evidence/*.json` finds only instance names | **P0-2** `evidence/hardware.md` manifest ([[13-restart-point]] step 0) |

## Medium severity (3)

| Note | One line | Owning action |
|---|---|---|
| [[root-only-cuts]] | GMI + MIR generated only before the tree loop → measured node reduction 0.0 % | **P0-3** in-tree cut loop (RW-1, [[ED-005-in-tree-cut-loop-not-more-cut-types]]) |
| [[docs-history-stale]] | `docs/history.md` stops at M5/Phase 4 while README/CHANGELOG claim Phases 5–6 | P1-7 / P2 doc consolidation ([[15-roadmap]]) |
| [[stale-build-artifacts]] | build trees still hold objects for deleted sources (`src/milp/cuts.cpp.o`) and removed targets | P2-1 hygiene pass; rebuild on demand |

## Low severity (5)

| Note | One line |
|---|---|
| [[changelog-gaps]] | versions 0.3.0–0.5.0 missing, three newest commits absent, pass-rate claim outruns evidence |
| [[dead-fuzz-target]] | `tests/fuzz/mps_coverage_fuzz.cpp` is never compiled by any CMake target |
| [[duplicate-benchmark-runners]] | `benchmarks/runners/*.py` are md5-identical copies of `scripts/*.py` |
| [[scratch-dirs-in-repo]] | eight gitignored `_m5-*` / `_deployment-*` / `_verify-*` trees plus `build/` at the repo root |
| [[verify-md-dangling-paths]] | `VERIFY.md` tells reviewers to regenerate a provenance manifest the repo does not contain |

## Bugs (2, in `docs/codebase/bugs/`)

| Note | Severity | One line | Owning action |
|---|---|---|---|
| [[blend-numerical-failure]] | **high** | on Netlib BLEND the simplex engine returns `NumericalFailure` while `scripts/run_gpu.py:241-245` records `pass=True` | **P0-2**: fix the status gate, add `gpu_status_gate` test ([[16-testing-evaluation-strategy]] §16.3 #7) |
| [[mps-parser-limitations]] | medium | free-format reader accepts a narrow MPS subset and hard-fails on unknown sections/records | P2-7 parser errors + QUADOBJ coverage tests ([[15-roadmap]]) |

## Cross-links: debt ↔ verdicts ↔ code

- **Verdicts:** [[12-keep-remove-rebuild]] — which components stay (§8.1), which are deleted (§8.2), which are rebuilt RW-1…RW-10 (§8.3), and the explicit "do not restart the core" answer (§8.4).
- **Inventory that produced these notes:** [[08-codebase-audit]] §4.1–§4.4 (directory verdicts, module verdicts, hidden-functionality check, doc-vs-code discrepancies).
- **Requirement impact of each debt:** [[09-research-code-alignment]] §6.1 (R1–R20 status) and [[21-traceability]] §21.1 (gap / recommendation / verification per requirement).
- **Component-level debt** lives beside each component note's *Open Questions / Risks* section, e.g. [[DualSimplexEngine]] (pricing not DSE), [[GPU-PDHG-Engine]] (measurement bug), [[BranchAndCut]] (root-only cuts).
- **Vault health:** [[05-vault-integrity]] — note counts, unresolved and ambiguous stems, alias convention.

## P2 cleanup items (from [[15-roadmap]] §P2)

| Item | Debt / bug notes it closes |
|---|---|
| **P2-1** Dead code & hygiene: delete `benchmarks/runners/*.py`, dead options (`power_iterations`, `node_strategy`, `infeasible_or_unbounded`), wire-or-remove the fuzz target, clean scratch dirs, restore or delete `data/qp` | [[duplicate-benchmark-runners]] · [[dead-fuzz-target]] · [[scratch-dirs-in-repo]] · [[stale-build-artifacts]] |
| **P2-7** MPS parser limitations (error messages, QUADOBJ coverage) | [[mps-parser-limitations]] |
| **P2-6** GPU CI job or demote GPU claims to "experimental" | [[blend-numerical-failure]] (regression guard), [[missing-hardware-metadata-in-evidence]] |
| **P1-7** Doc/claims consolidation: extend or archive `docs/history.md`, fix `VERIFY.md` paths, R1–R20 table in `STATUS.md` | [[docs-history-stale]] · [[verify-md-dangling-paths]] · [[changelog-gaps]] |
| **P0-1…P0-4** (not P2 — these are submission-blocking) | [[no-external-baseline]] · [[negative-parallel-scaling]] · [[root-only-cuts]] |

## Related

[[Codebase MOC]] · [[Architecture MOC]] · [[Evaluation MOC]] · [[SIH Strategy MOC]] · [[07-current-architecture]] (AP-1…AP-12) · [[15-roadmap]] · [[18-risk-register]]
