# SIH26119 submission package — v0.5.3

**Frozen revision:** tag `v0.5.3` (release commit `8017336`; the demonstration transcript is
captured against that revision's binaries). **Package date:** 2026-10-03. This document is the
index a reviewer uses to move from claims to artifacts: five evidence bundles (roadmap §10) and
the exact problem-statement requirement mapping. It is part of the submission work described in
the [SIH evidence preparation lane](../audit/INDUSTRY-GRADE-COMPETITIVE-ROADMAP.md).

**Scope in one paragraph.** markov-cero is a sovereign LP/MILP/QP/MIQP/NLP/MINLP solver core
with CLI, C++ API and Python bindings — built from scratch, no solver library dependency, Apache-2.0,
single-host qualified, honestly bounded: production and refinery gates stay open
([gate status](../../evidence/gate-status-20261003.json)); no speed claim is made
(IR-35 open); the rules confirmation is partial
([record](../../evidence/sih-rules-confirmation-20261003.json)) and the 2026-09-30 idea-submission
deadline has passed — confirm status with the college SPOC.

**Human authorship disclosure.** Implementation was produced with AI-agent assistance under human
direction; human decisions are recorded in `docs/decisions` and PROVENANCE (e.g. D18). Independent
provenance review (W00/D04) and release/math sign-offs (W02/W03, IR-33) are **pending, not claimed**.

## Evidence bundles

| # | Bundle (roadmap §10) | Primary artifacts |
|---|---|---|
| 1 | Problem and scope | This document's mapping table; [verbatim problem statement](../sih26119_problem_statement.md); [capability status](STATUS.md); [rules confirmation](../../evidence/sih-rules-confirmation-20261003.json) |
| 2 | Mathematics | [Demo transcript](../../evidence/qualification-demo-transcript-2026-10-03.txt) legs 1–2 (LP witness, MILP proof replay via `markov-cero-verify-mip`); [proof guarantee record](../../evidence/proof-guarantee-20260928.json); [MINLP OA proof replay](../../evidence/minlp02-proof-2026-10-01.json); [QP KKT checks](../../evidence/qp-kkt-bench-20260930.json); [numerical contract](../contracts/numerical-policy.md) |
| 3 | Robustness/scalability | Demo transcript legs 3–4 (validated Farkas conflict; honest `ResourceLimit` under a node cap); [resource envelope](../../evidence/resource-envelope-20260928.json); [device memory budget](../../evidence/device-memory-budget-2026-10-03.json); [BENCH-01…04](../../evidence/INDEX.md) with their recorded failures; [IR-19 memory record](../../evidence/ir19-w01-memory-20260928.json) |
| 4 | Competition | [QP agreement vs OSQP 1.1.3 / HiGHS 1.15.1](../../evidence/qp-kkt-bench-20260930.json); [MIP reference agreement](../../evidence/milp01-miplib-2026-10-01.json); [frozen comparators](../../evidence/frozen-comparators-20260928.json) and [instances](../../evidence/frozen-instances-20260928.json). **Limitation:** no held-out runtime-ratio claim — G4/G5 not met, IR-35 open |
| 5 | Sovereignty and usability | [PROVENANCE](PROVENANCE.md); [SPDX SBOM](../../evidence/release-sbom-20261001.json); [reproducible build](../../evidence/reproducible-build-20261001.json) (107/107 artifacts byte-identical); [offline qualification + install/rollback drills](../../evidence/packaging-qualification-20261001.json); [QUICKSTART](../guides/QUICKSTART.md), [BUILDING](../guides/BUILDING.md), [RELEASE](../guides/RELEASE.md); public repository |

**Suggested live demonstration** (roadmap §10; every leg has a runnable command and a captured
transcript): `bash scripts/run-qualification-demo.sh` → solve `data/miplib/stein9.mps` and replay
its `mip_proof` with `markov-cero-verify-mip` → solve `examples/refinery/refinery-infeasible.mps`
(validated conflict) → `stein9 --max-nodes 3` (resource-limited exit 5) → open a BENCH-04 record.
Do not present a prerecorded result as a live solve.

## Requirement mapping

Statuses: **done** = implemented and tested with named checks; **partial** = implemented or
measured with an explicit gap; **not claimed** = deliberately out of current evidence.

| # | Requirement (PS anchor) | Status | Evidence |
|---|---|---|---|
| R1 | Solver core, not a modeling environment | done | Library + CLI + Python; 123/123 CTest, 28/28 binding tests at `8017336` |
| R2 | LP, MILP, QP initial scope | done | Engines `primal/dual/ipm/pdlp/milp/qp/miqp`; [BENCH-01…04](../../evidence/INDEX.md), [MIP-01](../../evidence/milp01-miplib-2026-10-01.json), [QP-01](../../evidence/qp-kkt-bench-20260930.json) |
| R3 | Extensible to MIQP, NLP, MINLP (modular) | done | [MIQP-01](../../evidence/miqp01-synthetic-2026-10-01.json), [NLP-01/02](../../evidence/nlp02-restoration-2026-10-01.json), [MINLP-01/02](../../evidence/minlp01-oa-2026-10-01.json) |
| R4 | Revised simplex **and** interior-point | done | Primal/dual revised simplex (transcript leg 1), IPM + PDLP engines ([GAP-01](../../evidence/gap01-etamacro-2026-10-01.json) exercises all four) |
| R5 | Branch-and-bound/cut, cuts, presolve, heuristics, node selection | done | [MILP strengthening](../../evidence/milp-strengthening-20260928.json); CLI `--cuts/--heuristics/--node-selection/--branching` (ML-GNN rule is optional with documented fallback) |
| R6 | Sparse matrices + numerical linear algebra | done | Sparse-first canonicalization ([ED-004](../research/engineering-decisions/ED-004-sparse-first-canonicalization.md)); sparse basis factorization; Ruiz scaling |
| R7 | Multi-core parallelization | partial | MILP parallel tree search (`--threads`, STATUS feature table); benchmark evidence is single-thread by preregistered protocol — no parallel speedup claim |
| R8 | GPU **where measurable benefits** | partial | Optional CUDA path executes on real hardware ([device run](../../evidence/gpu-device-run-2026-10-02.json)) with a [device memory budget](../../evidence/device-memory-budget-2026-10-03.json); **no speedup established** (G6 not met, IR-28 open) |
| R9 | Numerical stability, convergence | partial | [Numerical contract](../contracts/numerical-policy.md) + boundary tests; campaigns record honest `NumericalFailure`s (e.g. BENCH-03/04 rows); breadth unproven |
| R10 | From scratch, no open-source solver library | done* | [PROVENANCE](PROVENANCE.md), Apache-2.0, no solver dependency; *independent provenance review (W00/D04) pending* |
| R11 | Industrial scope (refinery, planning, logistics…) | partial | [Refinery examples](../../examples/refinery) + qualification transcript leg 1; [units schema](../../evidence/refinery-units-schema-20260928.json); G8 refinery pilot **not done** (IR-34) |
| R12 | Industrial scale (thousands–millions of rows/cols) | not claimed | Large instances present; BENCH solved fraction 38/265 = 0.143 with full denominator — no industrial-scale solve claim |
| R13 | Degeneracy, ill-conditioning, difficult relaxations | partial | Adversarial families run and reported honestly; [IIS-domain test](../../tests), Farkas conflict (transcript leg 3); G2 adversarial-campaign record still open |
| R14 | API **or** CLI sufficient, no GUI required | done | CLI + C++ API + Python bindings (the optional web experience is not required) |
| R15 | MIPLIB / Netlib / Mittelmann benchmarks | partial | Checked-in subsets `data/miplib`, `data/netlib`, `data/mittelmann` measured in BENCH-01…04; 227 optional large files omitted by design (`scripts/datasets.py`) |
| R16 | Compare against ≥1 established solver | partial | QP agreement vs OSQP and HiGHS measured ([QP-01](../../evidence/qp-kkt-bench-20260930.json)); reference-optima agreement on MIP/LP; **no runtime-ratio comparison claim** (IR-35) |
| R17 | Numerical robustness on hard instances | partial | Honest failure evidence kept (etamacro [GAP-01](../../evidence/gap01-etamacro-2026-10-01.json), BENCH deferred set); no "solved the hard ones" claim |
| R18 | Transparent, extensible, sovereign foundation | done* | Apache-2.0, SBOM, reproducible build, public repo; *licence/dataset-rights decision D18 still partly pending* |
| R19 | Datasets: MIPLIB, Netlib, Mittelmann, QPLIB + refinery cases | partial | Subsets checked in with [provenance records](../../data); QPLIB subset in QP-01; refinery cases qualified; optional-dataset redistribution D18 pending |
| R20 | Consistently optimal/near-optimal, faster than weaker implementations | not claimed | G4/G5 not met; IR-35 open. Preregistered campaigns report measured statuses only |

## What is explicitly not claimed

Speed or superiority over any commercial or open-source solver; held-out performance improvement;
GPU acceleration benefits (execution is demonstrated, benefit is not); industrial plant data or
refinery engineer approval; conformance to the national evaluation rubric (unconfirmed);
signed artifacts, funded support ownership, or independent release/math review (IR-33 open).

## Open human tasks

1. **SPOC confirmation** of the team's submission status (the 2026-09-30 deadline has passed) and
   the evaluation rubric / later-phase dates ([rules record](../../evidence/sih-rules-confirmation-20261003.json)).
2. **Official SIH 2026 PPT** filled from this package (template: portal menu → Idea presentation template).
3. **Demo video** — non-AI-generated, narrated by team members (guidelines mirror; verify against official PDF).
4. **Independent reproduction** of the transcript on a second machine by a person not involved (roadmap lane step 4).
5. Optional: screenshots of the web experience and `markov-cero-info` for the deck.
