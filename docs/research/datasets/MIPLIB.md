---
type: concept
tags: [datasets, benchmark]
status: stable
verified_on: 2026-09-25
---

# MIPLIB

> The standard MIP library — collection for breadth, benchmark subset for the claims that get graded.

## Definition
MIPLIB is the community mixed-integer programming instance library; MIPLIB 2017 reorganized it into a 1065-instance *collection* plus a 240-instance *benchmark* set selected by data-driven criteria so that headline results are representative rather than cherry-picked. Instances are MPS files with known best solutions and bounds, and the benchmark subset is what solver papers report on. MIPLIB is deliberately heterogeneous (packing, scheduling, structural, combinatorial families) and is where degenerate and weakly formulated models concentrate — exactly the R13 territory. Correctness claims require verifying feasibility and objective against the recorded solution, not just trusting a status string.

## Why It Matters Here
- R15/R19 name MIPLIB first; R13's hard cases live here; R16 comparisons are expected on these instances.
- Observed state: `miplib_results.csv` contains **only 3 instances** and `netlib_results.csv`/`netlib_extended.csv` hold the bulk of results (docs/audit/00-ground-truth.md C.3) — Inference: MIPLIB coverage is far below what an evaluator would expect for a MILP claim.
- Observed state: an MPS parser exists (docs/codebase/components/MPSParser.md) and `miplib_benchmarks` is a registered CTest, so the pipeline is present; breadth is the gap.

## Key Facts / Rules
- MIPLIB 2017 = 1065 collection + 240 benchmark instances; report on the benchmark set for comparability.
- Always verify returned solutions independently (feasibility + objective), per the MIPLIB solution-checker practice.
- Performance variability is high on MIPLIB — multiple seeds/permutations needed before claiming differences (Lodi & Tramontani 2013).
- Distributional MIPLIB (D-MIPLIB 2024) exists to capture instance-level hardness variation for statistical claims.

## Related
- [[Relative Optimality Gap]]
- [[Geometric Mean Runtime]]
- [[Missing External Baseline Comparison]]
- [[Branch and Cut]]
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
- [[Achterberg-2005-MIPLIB-2003]]

## Referenced By

- [[18-risk-register|audit/18-risk-register]]
- [[21-traceability|audit/21-traceability]]
- [[Datasets MOC|research/Datasets MOC]]
- [[Research MOC|research/Research MOC]]
- [[No External Baseline|research/limitations/No External Baseline]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Geometric Mean Runtime|research/metrics/Geometric Mean Runtime]]
- [[Relative Optimality Gap|research/metrics/Relative Optimality Gap]]
- [[Achterberg-2005-MIPLIB-2003|research/papers/Achterberg-2005-MIPLIB-2003]]
- [[Applegate-2006-Traveling-Salesman-Problem|research/papers/Applegate-2006-Traveling-Salesman-Problem]]
- [[Chung-2015-Computational-Study-Cutting|research/papers/Chung-2015-Computational-Study-Cutting]]
- [[Gamst-1970s-MPS-format-specification|research/papers/Gamst-1970s-MPS-format-specification]]
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation|research/papers/Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
- [[Koch-n.d.-MIPLIB-Design-Experiments|research/papers/Koch-n.d.-MIPLIB-Design-Experiments]]
- [[Unknown-2024-Distributional-MIPLIB|research/papers/Unknown-2024-Distributional-MIPLIB]]
- [[Zhang-2025-Learning-Select-Nodes|research/papers/Zhang-2025-Learning-Select-Nodes]]
- [[Missing External Baseline Comparison|research/research-gaps/Missing External Baseline Comparison]]
- [[Mittelmann Coverage Gap|research/research-gaps/Mittelmann Coverage Gap]]