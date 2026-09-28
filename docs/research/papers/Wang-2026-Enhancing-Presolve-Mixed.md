---
type: paper
title: "Enhancing Presolve in Mixed Integer Programming by Combining Probing and Dual Fixing"
authors: "Wang, Chen & Dai"
year: 2026
venue: "arXiv preprint"
doi: "(unverified)"
domain: [presolve, milp]
priority: ✦
status: standard
tags: [paper, presolve]
---
# Enhancing Presolve in Mixed Integer Programming by Combining Probing and Dual Fixing
> Joint probing + dual fixing: use LP duals to pick probing candidates and probing results to strengthen fixing.
## Metadata
| Field | Value |
|---|---|
| Authors | Wang, Chen & Dai |
| Year | 2026 |
| Venue | arXiv preprint |
| DOI/URL | arXiv:2607.10767 |
## Problem Addressed
Probing is strong but expensive; dual fixing is cheap but weaker. Run separately they leave gains on the table and duplicate bound-propagation work. Combining them lets dual solutions steer where probing effort goes.
## Core Contribution
- **Methodology:** Use LP dual/reduced-cost information to rank variables for probing; feed probing implications back into fixing decisions; shared bound-propagation passes.
- **Assumptions:** Root LP solved (or partially solved) to obtain duals; bounded probing budget; binary/general-integer variables.
- **Benchmarks/datasets:** MIP benchmark sets (per arXiv paper; numbers not quoted here).
- **Metrics:** Fixed variables, root gap closed, nodes, time.
- **Key results:** Better reductions than either technique alone at controlled cost (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Probing scheduled by dual activity; dual fixing via [[Reduced Cost]] bounds.
**Techniques:** Candidate prioritization; shared propagation.
**Implementation details:** markov-cero already has dual fixing ingredients: `src/lp/dual/dual_simplex.cpp` produces duals and reduced costs, and heuristics (`src/milp/heuristics.cpp`) exist — but no fixing-by-reduced-cost rule runs in presolve.
**Limitations/failure cases:** Relies on decent LP duals; on very weak root relaxations duals are noisy — combine with [[Weak Relaxation]] awareness.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 (presolve + heuristics), R20 (near-optimal results quickly). Cheapest high-yield MIP presolve addition given duals are already computed at the root.
## Evidence → Engineering Decision
- *Finding:* Reduced-cost fixing is available from existing dual solves but unused in presolve → *PS requirement:* R5 → *Component:* src/presolve/presolve.cpp (post-root dual fixing; inputs from src/lp/dual/dual_simplex.cpp) → *Metric:* nodes explored; root [[Relative Optimality Gap]].
## Related Papers
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques]]
- [[Achterberg-2020-Presolve-Reductions-Mixed]]
## Uses
- [[Presolve]]
