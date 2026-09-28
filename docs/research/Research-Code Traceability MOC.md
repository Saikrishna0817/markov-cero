---
type: moc
tags: [moc, traceability, research, codebase]
status: complete
date: 2026-09-25
---

# Research-Code Traceability MOC

> How to walk this vault in **both directions**: from a paper to the line of code that implements
> it, and from a file to the literature that justifies it. The backing table for every path below
> is [[21-traceability]] (R1–R20 research / code / delta / verification); the status source it
> copies is [[09-research-code-alignment]] §6.1; the requirements themselves come from
> [[sih26119_problem_statement|SIH26119 Problem Statement]].

## The two hubs

- **[[Research MOC]]** — entry point for direction *Research → Code*: concept layer, 202-paper corpus, module map.
- **[[Codebase MOC]]** — entry point for direction *Code → Research*: 44 component/debt/decision notes with `file:line` evidence.
- **[[21-traceability]]** — the matrix they share: §21.1 requirement rows, §21.2 P0 closure register, §21.3 module map, §21.4 research finding → engineering decision, §21.5 the same navigation instructions in prose.

## Path A — Research → Code (start from a question about "what should exist")

1. **Paper** — open a note in `docs/research/papers/`; it states which requirement and technique it feeds.
2. **Technique / concept / algorithm** — follow into `docs/research/{techniques,concepts,algorithms}/`, e.g. [[Steepest Edge]] · [[Scaling]] · [[Dual Simplex]].
3. **Requirement** — the note's "Why It Matters Here" names the R number (R1–R20); read that row of [[21-traceability]] §21.1 for Status, Gap, Recommendation, Verification.
4. **Component note** — the Current-code column of the row points at a stem in `docs/codebase/components/` carrying exact `file:line` evidence.
5. **Audit matrix + actions** — close the loop through [[09-research-code-alignment]] (status), [[12-keep-remove-rebuild]] (RW-1…RW-10), [[13-restart-point]] (order) and [[15-roadmap]] (priority).

*Alternative shortcut:* start at [[cross-paper-synthesis]] §8 (the 12-item collective-knowledge verdict, each item already R-tagged) or [[research-dependency-map]] for prerequisite order.

## Path B — Code → Research (start from a file, ask "why is it shaped like this")

1. **Source file** → the component note that lists it in `source_files:` frontmatter (`docs/codebase/components/`).
2. **Component note → "Research Justification"** — the section that wikilinks out to algorithm/concept/technique notes; its *Open Questions / Risks* section names what is missing.
3. **Algorithm / concept note → papers** — the note's Related list cites 2–5 verified paper slugs from `docs/research/papers/`.
4. **Gap surfacing** — if nothing implements it, the link lands in `docs/research/research-gaps/` or `docs/research/limitations/` (e.g. [[No Interior-Point Engine]], [[Root-Only Cuts]]), which map back to the Status column of [[21-traceability]] §21.1.
5. **After any edit:** run `python3 scripts/link_backlinks.py` so every "Referenced By" block regenerates and the graph stays walkable ([[05-vault-integrity]] tracks the counts).

## Worked example 1 — dual simplex pricing (Code → Research → back to an action)

| Step | Follow | Lands on |
|---|---|---|
| 1 | source `src/lp/dual/dual_simplex.cpp` | [[DualSimplexEngine]] — Implementation Facts record `harris_ratio`, `tableau_norm`, condition trigger, caps |
| 2 | its **Research Justification** section | [[Dual Simplex]] — the technique note for the engine |
| 3 | that note's Related list | [[Steepest Edge]] — the pricing rule the engine does *not* have |
| 4 | the technique's cited papers | [[Goldfarb-1992-Steepest-Edge-Simplex]] · [[Goldfarb-1977-Practicable-Steepest-Edge]] · [[Fourer-1994-Steepest-Edge-Simplexing]] |
| 5 | requirement that needs it | **R13** → [[21-traceability]] §21.1 R13 (PARTIAL: anti-cycling floor exists, no steepest-edge) → [[09-research-code-alignment]] §6.2 "Steepest-edge dual pricing, P1" |
| 6 | action | [[12-keep-remove-rebuild]] **RW-7** (P1) → [[15-roadmap]] **P1-2**, with [[Bland-Only Pricing]] and [[Degeneracy Handling Gap]] as the honest current state |

## Worked example 2 — branch-and-cut separation (Code → Research → back to a P0)

| Step | Follow | Lands on |
|---|---|---|
| 1 | source `src/milp/milp_solver.cpp:165-208` | [[BranchAndCut]] — cuts applied only before the tree loop |
| 2 | its **Research Justification** + Related | [[Branch and Cut]] → [[Branch and Bound]] |
| 3 | algorithm note's cited papers | [[Cornuejols-2001-Branch-and-Cut-Algorithms]] · [[Achterberg-2005-General-Mixed-Integer]] · [[Padberg-1991-Branch-and-Cut-Algorithm]] |
| 4 | cut-family notes behind it | [[Gomory Mixed Integer Cut]] · [[Mixed Integer Rounding Cut]] · [[Cut Pooling]] · [[Cut Validity]] → [[CutGenerators]] component |
| 5 | requirement that needs it | **R5** → [[21-traceability]] §21.1 R5 (PARTIAL, 0.0 % node reduction) → [[09-research-code-alignment]] §6.2 "Cut regeneration inside tree, P0" |
| 6 | action | [[12-keep-remove-rebuild]] **RW-1** (P0) → [[15-roadmap]] **P0-3**, evidence target `evidence/cut_effectiveness.csv` (experiment **E6**), recorded as [[Root-Only Cuts]] |

## Quick index — which document answers what

| Question | Document |
|---|---|
| What did the PS require? | [[sih26119_problem_statement]] → [[00-ground-truth]] §A.2 (R1–R20) |
| What does the research say? | [[Research MOC]] → [[cross-paper-synthesis]] → `docs/research/papers/` |
| What does the code do? | [[Codebase MOC]] → component notes → [[07-current-architecture]] |
| Are they aligned, and where not? | [[09-research-code-alignment]] §6.1–6.2 → [[21-traceability]] §21.1 |
| What must be built/cut, in what order? | [[12-keep-remove-rebuild]] → [[13-restart-point]] → [[15-roadmap]] |
| How is closure proven? | [[16-testing-evaluation-strategy]] §16.6 → [[04-evidence-inventory]] |
| What is the health of this graph? | [[05-vault-integrity]] |

## Related

[[Architecture MOC]] · [[Algorithms MOC]] · [[Evaluation MOC]] · [[Datasets MOC]] · [[SIH Strategy MOC]] · [[Technical Debt MOC]] · [[cross-paper-synthesis]] · [[research-dependency-map]]
