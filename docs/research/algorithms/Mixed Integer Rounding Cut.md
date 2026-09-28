---
type: concept
tags: [algorithms, milp]
status: stable
verified_on: 2026-09-25
---

# Mixed Integer Rounding Cut

> The rounding-based workhorse inequality family: aggregate rows, then round fractional remainders into one valid cut.

## Definition
Mixed-integer rounding (MIR) inequalities come from taking a valid inequality for the mixed-integer set and applying a rounding function that is exact for integer right-hand sides: given an aggregated row with fractional right-hand side f₀, coefficients are mapped through a piecewise rounding function ψ(·, f₀) so that every integer point satisfying the row also satisfies the resulting inequality. In the binary single-row case the construction coincides with the Gomory mixed-integer cut; applied to aggregations and knapsacks it yields cover-style inequalities, which is why solvers generate it in both direct and complement orientations on aggregated rows. MIR is a validity-preserving *transformation*, so strength varies with the aggregation chosen.

## Why It Matters Here
- R5 requires cutting planes; MIR is the second implemented family and typically the more robust of the two on packing/covering structures common in refinery and logistics models (R11).
- Observed state: `generate_mir_cuts` applies `mir_function(a, f0)` in direct and complement orientations citing Marchand & Wolsey 2001, with a 1e-4 violation threshold, then shares `filter_cuts` with GMI (src/milp/mir.cpp:12-141, 222-228).
- Observed state: root-only separation, ≤10 kept cuts (src/milp/milp_solver.cpp:174-205) — Inference: the family's potential is under-used relative to its literature role.

## Key Facts / Rules
- Validity: ψ(·, f₀) is chosen so that ψ(a) ≤ ⌊a⌋-style rounding is respected for integer points — validity holds for *any* f₀ ∈ (0,1).
- Binary single-row MIR ≡ GMI cut; gains come from applying it to aggregated rows and knapsack covers.
- Aggregation choice dominates cut quality — the "which rows to merge" problem (see [[Rex-0000-Pool-Not-Row]]).
- Cuts must be re-validated in original space after canonicalization/presolve mapping.

## Related
- [[Gomory Mixed Integer Cut]]
- [[Cut Validity]]
- [[Weak Relaxation]]
- [[CutGenerators]]
- [[Marchand-1996-Mixed-Integer-Rounding]]
- [[Cornuejols-2008-Valid-Inequalities-Mixed]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[CutGenerators|codebase/components/CutGenerators]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Research-Code Traceability MOC|research/Research-Code Traceability MOC]]
- [[Gomory Mixed Integer Cut|research/algorithms/Gomory Mixed Integer Cut]]
- [[Cut Validity|research/concepts/Cut Validity]]
- [[Weak Relaxation|research/concepts/Weak Relaxation]]
- [[Root-Only Cuts|research/limitations/Root-Only Cuts]]
- [[Atamturk-2003-Cover-Inequalities-Mixed|research/papers/Atamturk-2003-Cover-Inequalities-Mixed]]
- [[Chvatal-1973-Edmonds-Polytopes-Hierarchy|research/papers/Chvatal-1973-Edmonds-Polytopes-Hierarchy]]
- [[Cornuejols-2008-Valid-Inequalities-Mixed|research/papers/Cornuejols-2008-Valid-Inequalities-Mixed]]
- [[Gomory-1963-All-Integer-Programming|research/papers/Gomory-1963-All-Integer-Programming]]
- [[Marchand-1996-Aggregation-Knapsack-Inequalities|research/papers/Marchand-1996-Aggregation-Knapsack-Inequalities]]
- [[Marchand-1996-Mixed-Integer-Rounding|research/papers/Marchand-1996-Mixed-Integer-Rounding]]
- [[Padberg-2005-Classical-Cuts-Mixed|research/papers/Padberg-2005-Classical-Cuts-Mixed]]
- [[Rex-0000-Pool-Not-Row|research/papers/Rex-0000-Pool-Not-Row]]
- [[Cut Pooling|research/techniques/Cut Pooling]]