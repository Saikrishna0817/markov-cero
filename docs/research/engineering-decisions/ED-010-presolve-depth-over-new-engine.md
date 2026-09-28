---
type: engineering-decision
id: ED-010
status: accepted
date: 2026-09-25
tags: [engineering-decision, presolve, r5, r13, p1]
---

# ED-010 — Presolve Depth Over a New Engine

> Spend the next engineering increment deepening the existing presolve rule set (implied bounds, forcing/dominated rows, duplicate rows, probing-lite) instead of building another solver engine; IPM is the only new engine authorized, by R4 (ED-003).

## Context (Observed fact)

- `include/markov_cero/presolve/presolve_stack.hpp:35-36` defines exactly four reduction records — `EmptyRow`, `EmptyColumn`, `FixedVariable`, `RowSingleton`; `src/presolve/presolve.cpp:67-177` runs those passes (`max_passes 5`) with a correct reversible LIFO `postsolve`.
- Against a research baseline of dozens of rules (probing, implied bounds, aggregation, dual fixing), 09 grades presolve at "3 rule classes" PARTIAL and RW-6 at P1.
- `cross-paper-synthesis` §1: presolve is the one component every surveyed solver has and none can remove, and Bixby attributes one of the four big multiplicative LP speedups to it.
- After ED-002/ED-003 no LP/MILP engine is missing from scope — additional engines would be parallel work with no PS clause demanding them.

## Research Evidence

- [[Achterberg-2020-Presolve-Reductions-Mixed]] — the modern reduction catalogue and rule ordering.
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques]] — probing: the highest-yield missing rule.
- [[Andersen-1995-Presolving-Linear-Programming]] — LP-side presolve interacting with dual recovery.
- [[Presolve]] — concept note tying rules to the reversible postsolve contract.
- [[Presolve-Postsolve Stack]] — architecture note: stack order must survive every new rule.

## Decision

- Add, in this order and inside the existing stack: implied bound propagation, forcing/dominated rows, duplicate row/column detection, then probing-lite with a bounded probe budget.
- Every new reduction appends to the same LIFO `ReductionRecord` stack so `postsolve` stays exact; each rule ships with its own postsolve verification cases and can be disabled independently.
- Guard the known failure mode: aggressive reductions degrade conditioning and can break answers beyond tolerance — no rule lands without a test.
- Defer any *new engine* beyond ED-003's IPM, and defer new cut families (ED-005 puts them at P2).

## Consequences (positive / negative / neutral)

- **positive:** multiplicative solve-time wins feeding R5/R13/R20; low-risk, testable increments; measurable as presolved nnz/row reduction and before/after solve times in the ED-001 harness.
- **negative:** conditioning risk from aggressive reductions; more postsolve edge cases to test.
- **neutral:** engine code, verifiers and the CLI interface are untouched.

## Alternatives Rejected

- *New LP engine (another first-order variant)* — no PS clause requires it; duplicates dispatch.
- *More cut families first* — ED-005 shows placement matters more than variety; both are tree-side.
- *Leave presolve at 3 rules* — leaves R5/R13 weak while the cheapest speedup goes unused.

## Linked Requirements

- R5, R9, R13, R20 → [[sih26119_problem_statement]]

## Related

- [[09-research-code-alignment]] (R5, §6.2) · [[12-keep-remove-rebuild]] (RW-6) · [[13-restart-point]] (step 8) · [[21-traceability]] §21.4 row 8
- [[Presolve]] · [[ED-004-sparse-first-canonicalization]] · [[ED-005-in-tree-cut-loop-not-more-cut-types]]
