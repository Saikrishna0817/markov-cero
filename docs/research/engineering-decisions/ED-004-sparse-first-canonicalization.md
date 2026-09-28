---
type: engineering-decision
id: ED-004
status: accepted
date: 2026-09-25
tags: [engineering-decision, canonicalization, sparsity, r6, r12, p1]
---

# ED-004 — Sparse-First Canonicalization, One Path

> Collapse the two canonicalization paths into a single sparse-first path; the dense route survives only as an internal fast path behind the same interface, never as a caller-visible branch.

## Context (Observed fact)

- Two divergent implementations: dense `canonicalize(model)` (`src/transform/canonicalize.cpp:48`) and `sparse_canonicalize(model, relax_integrality)` (`src/transform/sparse_canonicalize.cpp:119`), selected by a dense gate of 2048×8192 with `maximum_canonical_dimension = 8192` hardcoded at `src/transform/canonicalize.cpp:8`.
- 09 §6.3 grades the gate "convenience threshold, no research basis, creates two paths → REWRITE to sparse-first"; `12` RW-5 makes it P1.
- Downstream [[DenseLU]] and the reference engines densify node LPs via `to_dense()` (`include/markov_cero/transform/sparse_canonical_model.hpp:24`), so the large-scale path is the least-tested one — R12 risk.

## Research Evidence

- [[CSC Sparse Model]] — one immutable CSC model feeds every engine.
- [[Sparsity]] — canonical storage must be sparse before any engine sees it.
- [[Bell-2008-Efficient-Sparse-Matrix]] — sparse structures and the compounding cost of dense detours.
- [[Cheshmi-2017-Transforming-Sparse-Matrix]] — reordering/transformation choices belong at build time.
- [[Hall-2005-Hyper-sparsity-Revised-Simplex]] — sparsity pays only if it is preserved through every stage.

## Decision

- Make `sparse_canonicalize` the only path invoked by the pipeline; delete the 2048×8192 caller gate. Dense storage stays only *inside* engines that require it (reference simplex workspace), reached through `to_dense()`.
- Raise reference-engine dimension caps into options/config (RW item: "raise caps to config") so scale limits are visible, reported and testable instead of silent.
- One shared test suite drives both scales: existing dense-scale cases plus a ≥1e5-row sparse case.

## Consequences (positive / negative / neutral)

- **positive:** a single tested path at R12 scale; removes an arbitrary threshold; halves the canonicalization surface an evaluator must audit; matches the target architecture.
- **negative:** migration touches callers of `canonicalize()` (presolve, MILP, pump); tiny dense-only models may pay a small constant-factor cost.
- **neutral:** MPS parsing, postsolve reconstruction and the verifiers are unaffected.

## Alternatives Rejected

- *Keep the gate* — convenience threshold with no research basis (09 §6.3).
- *Fully dense rewrite* — contradicts R6/R12 ("sparse matrix techniques").
- *New storage/reordering engine now* — scope creep; ordering work belongs in the factorization path (R6).

## Linked Requirements

- R6, R12, R3 → [[sih26119_problem_statement]]

## Related

- [[09-research-code-alignment]] (R6, R12, §6.3) · [[12-keep-remove-rebuild]] (RW-5) · [[13-restart-point]] (step 8) · [[21-traceability]] §21.1 R6/R12
- [[Canonicalizer]] · [[ED-010-presolve-depth-over-new-engine]]
