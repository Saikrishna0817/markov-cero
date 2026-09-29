---
type: metric
tags: [metrics, milp]
status: stable
verified_on: 2026-09-25
---

# Cut Efficiency

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> How much bound and node reduction a cut buys per unit of separation cost — the metric that decides whether cutting planes pay.

## Definition
Cut efficiency measures return on investment for cut separation: on the strength side, per-cut *efficacy* (violation of the fractional point divided by the norm of the cut's coefficient vector) predicts how much a cut tightens the relaxation; on the system side, the aggregate is root gap closed by cuts, node-count reduction versus no-cuts runs, and LP re-solve cost spent obtaining them. A cut that is valid but parallel/redundant to an existing one adds rows without adding bound, so modern codes deduplicate (cosine/parallelism filters) and cap counts. Reporting cuts generated alone is not a metric — generated *and* kept *and* the bound change they produced is.

## Why It Matters Here
- R5 requires cutting planes; R20 is about time, so a cut family that never reduces time is decoration.
- Observed state: `filter_cuts` sorts by efficacy, drops violations < 1e-4 and cosine similarity > 0.95, caps at 10 cuts (src/milp/cut_pool.cpp:62-100); counts surface as `Result::cuts_generated` (src/milp/milp_solver.cpp:184).
- Observed state: `evidence/benchmarks/phase4.json` shows cut-driven node reduction **0.0%** — Inference: measured efficiency is currently zero because cuts run only at the root and only once (see [[Root-Only Cuts]]), not because the families are wrong.

## Key Facts / Rules
- Efficacy = |violation| / ‖coefficients‖ (2-norm) — the standard per-cut strength proxy for ranking.
- Aggregate metrics: root gap closed (before/after bound), nodes with cuts ÷ nodes without, CPU spent separating.
- Redundancy filter: reject cuts nearly parallel to accepted ones (max_parallelism 0.95 cosine here).
- Always count *kept* cuts, not generated candidates — generation volume is not effectiveness.

## Related
- [[Cut Validity]]
- [[Gomory Mixed Integer Cut]]
- [[Root-Only Cuts]]
- [[Weak Relaxation]]
- [[Turner-2024-Potential-Cutting-Planes]]

## Referenced By

- 16-testing-evaluation-strategy
- 21-traceability
- Architecture MOC
- Evaluation MOC
- Research MOC
- [[Cut Validity|research/concepts/Cut Validity]]
- [[ED-005-in-tree-cut-loop-not-more-cut-types|research/engineering-decisions/ED-005-in-tree-cut-loop-not-more-cut-types]]
- [[Root-Only Cuts|research/limitations/Root-Only Cuts]]
- cross-paper-synthesis
- [[Atamturk-2003-Cover-Inequalities-Mixed|research/papers/Atamturk-2003-Cover-Inequalities-Mixed]]
- [[Balas-1980-Cuts-Fixed-Rank|research/papers/Balas-1980-Cuts-Fixed-Rank]]
- [[Balas-1993-Lift-Project-Cutting|research/papers/Balas-1993-Lift-Project-Cutting]]
- [[Balas-1996-Gomory-Cuts-Revisited|research/papers/Balas-1996-Gomory-Cuts-Revisited]]
- [[Benichou-1997-Linear-Programming-Implementations|research/papers/Benichou-1997-Linear-Programming-Implementations]]
- [[Chung-2015-Computational-Study-Cutting|research/papers/Chung-2015-Computational-Study-Cutting]]
- [[Cornuejols-2001-Branch-and-Cut-Algorithms|research/papers/Cornuejols-2001-Branch-and-Cut-Algorithms]]
- [[Cornuejols-2008-Valid-Inequalities-Mixed|research/papers/Cornuejols-2008-Valid-Inequalities-Mixed]]
- [[Giallombardo-2025-Machine-Learning-Techniques|research/papers/Giallombardo-2025-Machine-Learning-Techniques]]
- [[Gomory-1958-Outline-Algorithm-Integer|research/papers/Gomory-1958-Outline-Algorithm-Integer]]
- [[Gomory-1963-All-Integer-Programming|research/papers/Gomory-1963-All-Integer-Programming]]
- [[Kimiaei-2025-Machine-Learning-Algorithms|research/papers/Kimiaei-2025-Machine-Learning-Algorithms]]
- [[Marchand-1996-Mixed-Integer-Rounding|research/papers/Marchand-1996-Mixed-Integer-Rounding]]
- [[Padberg-1991-Branch-and-Cut-Algorithm|research/papers/Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Padberg-2005-Classical-Cuts-Mixed|research/papers/Padberg-2005-Classical-Cuts-Mixed]]
- [[Pochet-2006-Production-Planning-Mixed|research/papers/Pochet-2006-Production-Planning-Mixed]]
- [[Rex-0000-Pool-Not-Row|research/papers/Rex-0000-Pool-Not-Row]]
- [[Richard-2010-Group-Approach-Cutting|research/papers/Richard-2010-Group-Approach-Cutting]]
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques|research/papers/Savelsbergh-1994-Preprocessing-Probing-Techniques]]
- [[Su-2025-Investigating-Exact-Effectiveness|research/papers/Su-2025-Investigating-Exact-Effectiveness]]
- [[Turner-2024-Potential-Cutting-Planes|research/papers/Turner-2024-Potential-Cutting-Planes]]