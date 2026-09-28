---
type: engineering-decision
id: ED-002
status: accepted
date: 2026-09-25
tags: [engineering-decision, simplex, first-order, r4, r10, keep]
---

# ED-002 — Keep the Simplex Core, Add First-Order Methods, Do Not Replace

> The certified revised/dual simplex core stays; first-order and interior-point engines are added alongside it, and Bland anti-cycling is demoted to a fallback — never removed.

## Context (Observed fact)

- `12-keep-remove-rebuild` §8.1 KEEP lists the revised simplex core, the dual simplex + Harris node engine and the PDLP/PDHG CPU engine with Ruiz scaling as sound and tested.
- `src/lp/reference/revised_simplex.cpp:383` certifies every terminal result; `src/lp/dual/dual_simplex.cpp:363` warm-starts from a `BasisState` — together they are the MIP node workhorse (`src/milp/node_lp.cpp:45-77`).
- The corpus never suggests deleting anti-cycling: `cross-paper-synthesis` §6 records that Bland is to be *demoted to a stall-detected fallback*, not replaced.
- `14-target-architecture` non-goals: no rewrite of simplex/ADMM/verifier cores — they pass audit.

## Research Evidence

- [[Revised Simplex]] — cold-solve reference method (Phase-I/II, Farkas certificates).
- [[Dual Simplex]] — warm-start re-optimization; why it exists as the node LP engine.
- [[Primal-Dual Hybrid Gradient]] — the first-order complement already shipped as [[PDLP-Engine]].
- [[Bland-Only Pricing]] — correct architecture: good pricing default, Bland as the guarantee.
- [[Bixby-2002-Evolution-of-LP]] — dual simplex and steepest-edge are speedups *inside* simplex, so improve the core rather than swap it out.

## Decision

- No rewrite of the simplex, ADMM or verifier cores. New capability lands inside or beside them: dual steepest-edge pricing (RW-7), a sparse IPM + crossover engine (ED-003), iterative refinement in the LU solves (RW-8).
- Bland pricing keeps its current default and stays the stall-detected fallback after steepest-edge lands; `bland_anti_cycling` is never deleted from the options.
- First-order PDLP/PDHG is dispatched by size/accuracy policy and is not allowed to displace simplex as the MIP node engine — node LPs need a basis, which first-order methods do not produce.

## Consequences (positive / negative / neutral)

- **positive:** preserves tested, certified code and the warm-start chain; removes rewrite risk against a 5-day window (rules 10/11).
- **negative:** reference caps (1024×8192, 4M elements, dense workspace) remain, so R12 scale depends on ED-004 and on honest engine dispatch.
- **neutral:** three LP engines to document; the dispatch policy must be stated in STATUS.

## Alternatives Rejected

- *Rewrite simplex from scratch* — no evidence the core is wrong; guaranteed deadline miss (12 §8.4).
- *IPM-only solver* — no basis ⇒ no warm starts, no crossover, broken MIP architecture.
- *Link an external LP library* — direct violation of R10/C1 — and *delete Bland*, which turns a performance problem into a non-termination bug.

## Linked Requirements

- R2, R4, R5, R9, R10 → [[sih26119_problem_statement]]

## Related

- [[09-research-code-alignment]] · [[12-keep-remove-rebuild]] (§8.1, RW-7) · [[13-restart-point]] (step 8) · [[21-traceability]] §21.1 R2/R4/R13
- [[ED-003-interior-point-required-by-ps]] · [[ED-004-sparse-first-canonicalization]]
