---
type: concept
tags: [concepts, lp]
status: stable
verified_on: 2026-09-25
---

# Basic Solution

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Set the nonbasics to zero and solve the square system — the vertex from which every simplex move departs.

## Definition
Given a basis B of Ax = b, the basic solution assigns x_N = 0 (or the nonbasic bound) and computes x_B = B⁻¹b. If additionally x_B ≥ 0 it is a basic *feasible* solution, and the fundamental theorem of LP says every extreme point of the feasible polyhedron is some basic feasible solution and vice versa (for nondegenerate bases). A basic solution with a zero component in x_B is degenerate, and that vertex may then be described by several different bases. Phase-I constructs an artificial basis so that a basic *feasible* starting point exists even when the original system has none.

## Why It Matters Here
- R4/R9 hinge on producing correct vertices: the reference engine's Phase-I/Phase-II structure exists precisely to obtain a BFS and to detect infeasibility.
- Observed state: phase-I artificials are added unless a crash basis succeeds, and a positive phase-I optimum is converted into a Farkas certificate (src/lp/reference/revised_simplex.cpp:276-434).
- Observed state: cut generation needs a basic solution's tableau rows — `generate_gomory_cuts` takes the `BasisState` and skips rows with f₀ outside [min_fractionality, 1−min_fractionality] (src/milp/gomory.cpp:12-96).

## Key Facts / Rules
- Vertices ↔ basic feasible solutions (one vertex may map to several bases when degenerate).
- Count of candidate bases is C(n, m); not all give feasible solutions.
- x_B = B⁻¹b, x_N = 0; feasibility ⇔ B⁻¹b ≥ 0.
- Phase-I optimality at value 0 ⇒ BFS exists; value > 0 ⇒ original system infeasible (Farkas ray).

## Related
- [[Basis]]
- [[Degeneracy]]
- [[Duality Gap]]
- [[Revised Simplex]]
- [[Vanderbei-1996-Foundations-and-Extensions]]

## Referenced By

- [[Basis|research/concepts/Basis]]
- [[Dantzig-1963-Linear-Programming-Extensions|research/papers/Dantzig-1963-Linear-Programming-Extensions]]