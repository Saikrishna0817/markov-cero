---
type: engineering-decision
id: ED-003
status: accepted
date: 2026-09-25
tags: [engineering-decision, interior-point, crossover, r4, p0]
---

# ED-003 — Interior-Point Is Required by the PS (Add It or Re-Scope Formally)

> R4 names interior-point methods explicitly, so a sparse primal-dual IPM **with crossover to a basis** gets built (or the requirement is formally re-scoped with evidence) — PDHG does not count.

## Context (Observed fact)

- `rg` for interior-point / barrier / Mehrotra across `src/`, `include/`, `apps/` → 0 hits; crossover → 0 hits (the only "crossover" in the repo is `scripts/plot_crossover.py`, a CPU/GPU scale study). **GAP (hard)**, 09 R4.
- `00-ground-truth` C.3: the README claims "interior-point"; the verified state is that PDLP is a first-order method — a different algorithm class ([[First-Order Accuracy Ceiling]]).
- `13-restart-point` step 6: 2–4 days, the long pole; decide day 1, *after* the harness so we know where an IPM would matter.

## Research Evidence

- [[Karmarkar-1984-New-Polynomial-Time-Algorithm]] — origin of the requirement's wording.
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]] — predictor-corrector: the practical sparse primal-dual algorithm modern LP IPMs are built on.
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]] — implementation pitfalls: scaling, dense columns, termination tests.
- [[Ye-1998-Crossover-Interior-Point]] — crossover turns the interior point into a vertex + basis.
- [[No Interior-Point Engine]] — the named gap note, paired with [[No Crossover]].

## Decision

- Implement a **minimal sparse Mehrotra-style primal-dual IPM** for small/medium LPs behind `--engine ipm`, plus a crossover phase that hands the resulting basis to [[DualSimplexEngine]] for polishing and node warm starts.
- Every crossover basis must pass [[IndependentVerifiers]] before any `optimal` status is reported — same dual gate as the simplex engines (ED-008).
- Sequence: harness first (ED-001), then IPM. If by day 3 no measured instance class benefits, a **formal re-scope** is written into STATUS with the harness data attached — the only acceptable substitute.
- Explicitly rejected as a substitute: relabelling PDHG/PDLP as "interior-point".

## Consequences (positive / negative / neutral)

- **positive:** closes the second hard GAP; adds a basis-producing large-LP path and the IPM → dual simplex complementarity the corpus recommends.
- **negative:** schedule's long pole (2–4 days); IPM accuracy is conditioning-limited and needs RW-8 refinement tooling to be credible.
- **neutral:** a fourth LP engine to document; the dispatch policy must be measured, not guessed.

## Alternatives Rejected

- *Do nothing / prose justification* — R4 is named in the PS text and graded (00 §A.7).
- *Call PDLP the IPM* — factually wrong class, contradicted by `evidence/benchmarks/crossover_study.csv`.
- *Dense Karmarkar-era IPM or crossover-less IPM* — unsuitable for sparse industrial models (09 §6.4); IPM without crossover is not R4.

## Linked Requirements

- R4, R5, R20 → [[sih26119_problem_statement]]

## Related

- [[09-research-code-alignment]] (R4, §6.2) · [[12-keep-remove-rebuild]] · [[13-restart-point]] (step 6) · [[21-traceability]] §21.1 R4, §21.2
- [[ED-001-comparison-harness-before-new-algorithms]] · [[ED-002-keep-simplex-core-add-first-order-not-replace]] · [[ED-008-retain-zero-trust-verifiers]]
