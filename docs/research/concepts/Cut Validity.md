---
type: concept
tags: [concepts, milp]
status: stable
verified_on: 2026-09-25
---

# Cut Validity

> A cut that is not valid cuts off optimal solutions — a correctness property, not a performance tuning knob.

## Definition
A valid inequality holds for every point of the integer-feasible set (ideally for every point of its convex hull, the integer hull). Cutting-plane separation removes fractional points from the *relaxation* while leaving all integer-feasible points inside; facet-defining cuts are the strongest valid ones, and the weaker "valid but not facet" class still matters because it is cheap to generate. Cuts added at a node are often only *locally* valid — they may be cut off by a later branch — so the pool must be managed per subtree. Numerically, a cut must be validated against a tolerance: a cut that "violates" by 1e-16 is a rounding artifact, not a separation.

## Why It Matters Here
- R5 requires cutting planes; an invalid cut silently changes the problem and invalidates every certificate — directly threatening R17/R20 claims.
- Observed state: `filter_cuts` keeps only non-redundant candidates (efficacy sort, `min_violation 1e-4`, cosine `max_parallelism 0.95`, `max_cuts 10`) and a GMI cut is kept only if it has nonzeros and cuts the incumbent (src/milp/gomory.cpp:202-213; src/milp/cut_pool.cpp:62-100).
- Inference (open question in docs/codebase/components/CutGenerators.md): there is no independent re-verification of cut validity against the relaxation beyond the violation test.

## Key Facts / Rules
- Validity test: no integer-feasible point violates the inequality; GMI/MIR validity follows from a floor/rounding argument on a single row.
- Local validity: cuts proven only under node bounds must be dropped or guarded when the bound is relaxed.
- Numerical hygiene: re-check violation against the original (unscaled) model after scaling/presolve maps the cut back.
- Strength ≠ validity: all separating cuts are valid; only some are facets.

## Related
- [[Gomory Mixed Integer Cut]]
- [[Mixed Integer Rounding Cut]]
- [[LP Relaxation]]
- [[Cut Efficiency]]
- [[Cornuejols-2008-Valid-Inequalities-Mixed]]

## Referenced By

- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Research-Code Traceability MOC|research/Research-Code Traceability MOC]]
- [[Branch and Cut|research/algorithms/Branch and Cut]]
- [[Gomory Mixed Integer Cut|research/algorithms/Gomory Mixed Integer Cut]]
- [[Mixed Integer Rounding Cut|research/algorithms/Mixed Integer Rounding Cut]]
- [[LP Relaxation|research/concepts/LP Relaxation]]
- [[Weak Relaxation|research/concepts/Weak Relaxation]]
- [[ED-005-in-tree-cut-loop-not-more-cut-types|research/engineering-decisions/ED-005-in-tree-cut-loop-not-more-cut-types]]
- [[Root-Only Cuts|research/limitations/Root-Only Cuts]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Cut Efficiency|research/metrics/Cut Efficiency]]
- [[Achterberg-2007-Constraint-Integer-Programming|research/papers/Achterberg-2007-Constraint-Integer-Programming]]
- [[Atamturk-2003-Cover-Inequalities-Mixed|research/papers/Atamturk-2003-Cover-Inequalities-Mixed]]
- [[Balas-1980-Cuts-Fixed-Rank|research/papers/Balas-1980-Cuts-Fixed-Rank]]
- [[Balas-1993-Lift-Project-Cutting|research/papers/Balas-1993-Lift-Project-Cutting]]
- [[Balas-1996-Gomory-Cuts-Revisited|research/papers/Balas-1996-Gomory-Cuts-Revisited]]
- [[Benichou-1997-Linear-Programming-Implementations|research/papers/Benichou-1997-Linear-Programming-Implementations]]
- [[Chvatal-1973-Edmonds-Polytopes-Hierarchy|research/papers/Chvatal-1973-Edmonds-Polytopes-Hierarchy]]
- [[Cornuejols-2001-Branch-and-Cut-Algorithms|research/papers/Cornuejols-2001-Branch-and-Cut-Algorithms]]
- [[Cornuejols-2008-Valid-Inequalities-Mixed|research/papers/Cornuejols-2008-Valid-Inequalities-Mixed]]
- [[Gomory-1958-Outline-Algorithm-Integer|research/papers/Gomory-1958-Outline-Algorithm-Integer]]
- [[Gomory-1963-All-Integer-Programming|research/papers/Gomory-1963-All-Integer-Programming]]
- [[Jabbar-2024-Cut-Based-Conflict-Analysis|research/papers/Jabbar-2024-Cut-Based-Conflict-Analysis]]
- [[Marchand-1996-Aggregation-Knapsack-Inequalities|research/papers/Marchand-1996-Aggregation-Knapsack-Inequalities]]
- [[Marchand-1996-Mixed-Integer-Rounding|research/papers/Marchand-1996-Mixed-Integer-Rounding]]
- [[Nemhauser-1988-Integer-Combinatorial-Optimization|research/papers/Nemhauser-1988-Integer-Combinatorial-Optimization]]
- [[Padberg-1991-Branch-and-Cut-Algorithm|research/papers/Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Padberg-2005-Classical-Cuts-Mixed|research/papers/Padberg-2005-Classical-Cuts-Mixed]]
- [[Rex-0000-Pool-Not-Row|research/papers/Rex-0000-Pool-Not-Row]]
- [[Richard-2010-Group-Approach-Cutting|research/papers/Richard-2010-Group-Approach-Cutting]]
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques|research/papers/Savelsbergh-1994-Preprocessing-Probing-Techniques]]
- [[Su-2025-Investigating-Exact-Effectiveness|research/papers/Su-2025-Investigating-Exact-Effectiveness]]
- [[Cut Pooling|research/techniques/Cut Pooling]]