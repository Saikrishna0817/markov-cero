---
type: moc
tags: [moc, codebase, architecture]
status: stable
verified_on: 2026-09-25
---

# Codebase MOC — markov-cero knowledge graph

> Map of content for `docs/codebase/`: the 44 notes describing what this repository
> demonstrably does, anchored to the audit trail above it and the research layer beside it.

## How to navigate

Start at [[07-current-architecture]] for the as-built pipeline, then drill into any component
note below — each one carries `source_files:` line references into `src/`, `apps/` or `gpu/`.
Gaps live in **Technical Debt** and **Bugs**; the *why* behind a design choice lives in
**Decisions**. Every claim worth trusting is graded in [[04-evidence-inventory]], and the health
of this very graph (counts, unresolved and ambiguous stems) is tracked in [[05-vault-integrity]].
When a note here contradicts the literature, check the concept layer through [[Research MOC]].

## Four sources of truth

Problem statement, research literature, current codebase and target design are four different
authorities and must not be blended. Read [[00-ground-truth]] first — it defines the labels
([A]–[D]) every later audit note cites.

## Architecture

- [[07-current-architecture]] — Phase 5 reconstruction: the system exactly as built.
- Pipeline spine, in execution order: [[MPSParser]] → [[Canonicalizer]] →
  [[codebase/components/Presolve|Presolve]] → [[RuizScaling]] → engine dispatch →
  postsolve → [[Solution-JSON-Writer]].
- Cross-cutting notes: [[Solve-Pipeline]] (data flow), [[csc-sparse-storage]] (storage layout),
  [[numerical-policy-centralized]] (floating-point policy), [[status-certificate-fail-closed]].

## Components

### Continuous LP engines
- [[RevisedSimplexEngine]] — reference Phase-I/II simplex, `src/lp/reference/revised_simplex.cpp`.
- [[DualSimplexEngine]] — dual simplex, `src/lp/dual/dual_simplex.cpp`.
- [[PDLP-Engine]] — first-order primal-dual hybrid gradient, `src/lp/first_order/pdlp.cpp`.
- [[GPU-PDHG-Engine]] — GPU PDHG kernel path, `gpu/src/pdhg_step.cpp`.

### Sparse linear algebra
- [[SparseBasis-LU]] — basis-matrix LU, `src/linalg/sparse_basis.cpp`.
- [[DenseLU]] — dense LU fallback, `src/linalg/dense_lu.cpp`.
- [[LDL-Factorization]] — LDLᵀ on the QP KKT system, `src/qp/kkt.cpp`.

### Model pipeline
- [[MPSParser]] — free-format MPS reader, `src/io/mps.cpp`.
- [[Canonicalizer]] — canonical forms, `src/transform/canonicalize.cpp`.
- [[codebase/components/Presolve|Presolve]] — reductions + LIFO postsolve stack, `src/presolve/presolve.cpp`.
- [[RuizScaling]] — infinity-norm equilibration, `src/scale/ruiz_scaling.cpp`.

### MILP engine
- [[BranchAndCut]] — branch-and-cut driver, `src/milp/milp_solver.cpp`.
- [[CutGenerators]] — Gomory and friends, `src/milp/gomory.cpp`.
- [[PrimalHeuristics]] — rounding / feasibility heuristics, `src/milp/heuristics.cpp`.
- [[ParallelTreeSearch]] — work-stealing tree search, `src/milp/parallel_tree_search.cpp`.

### QP engine
- [[QP-ADMM-Engine]] — ADMM QP solver, `src/qp/admm_solver.cpp`; KKT via [[LDL-Factorization]].

### Verification
- [[IndependentVerifiers]] — reference LP verifier, `src/verify/reference_lp_verifier.cpp`.
- [[Solution-JSON-Writer]] — machine-checkable solution + status certificate, `apps/json_output.hpp`.

### CLI
- [[CLI-MarkovCeroSolve]] — `--engine auto|primal|dual|pdlp|milp|qp|miqp|parallel` entry point.
- [[CLI-InfoAndMpsInspect]] — inspection subcommands over a model file.
- No dedicated CLI component note exists yet (documentation gap); its output writer is
  [[Solution-JSON-Writer]].

## Data Flow

- [[Solve-Pipeline]] — MPS → canonicalize → presolve/scale → engine → postsolve → JSON.
- The stage-by-stage picture with source line references is in [[07-current-architecture]] §C.1.

## APIs

- [[Library-API]] — the C++ library surface consumed by embedders.
- [[CLI-MarkovCeroSolve]] · [[CLI-InfoAndMpsInspect]] — the command-line surface.

## Tests

- [[overview]] — what the suite actually covers today.
- [[benchmark-suites]] — Netlib / MIPLIB / Mittelmann harnesses.
- [[testing-gaps]] — what is *not* covered, and why it matters.

## Technical Debt

Eleven open notes:
- [[changelog-gaps]] · [[dead-fuzz-target]] · [[docs-history-stale]] · [[duplicate-benchmark-runners]]
- [[missing-hardware-metadata-in-evidence]] · [[negative-parallel-scaling]] · [[no-external-baseline]]
- [[root-only-cuts]] · [[scratch-dirs-in-repo]] · [[stale-build-artifacts]] · [[verify-md-dangling-paths]]

## Bugs

- [[blend-numerical-failure]] — silent numerical failure mode during blending.
- [[mps-parser-limitations]] — what the free-format reader rejects.

## Decisions

- [[clean-room-provenance]] · [[csc-sparse-storage]] · [[no-external-solver-dependency]]
- [[numerical-policy-centralized]] · [[status-certificate-fail-closed]]

## Evidence

- [[04-evidence-inventory]] — every evidence and report artifact, what it measures, how many
  instances, whether hardware metadata is recorded, and which requirement it supports.

## Integrity

- [[05-vault-integrity]] — note counts per folder, total wikilinks, unresolved and ambiguous
  stems, and the alias convention this MOC follows (path-qualified targets for shared stems).

## Research side

- [[Research MOC]] — the concept layer and the paper corpus behind these components.
- Research-side architecture notes: [[Multi-Engine Solver Architecture]],
  [[Presolve-Postsolve Stack]], [[CSC Sparse Model]].
- [[00-ground-truth]] — the four sources of truth, restated for the codebase graph.
