---
type: codebase-tech-debt
tags: [codebase, technical-debt, evidence, benchmarks]
severity: high
status: verified
verified_on: 2026-09-25
evidence:
  - "evidence/benchmarks/"
  - "reports/"
  - "docs/audit/00-ground-truth.md:188"
  - "docs/sih26119_problem_statement.md:77"
---

# No External Baseline

> Nothing in evidence compares markov-cero against HiGHS, CPLEX, Gurobi, CBC, or SCIP — PS requirement R16 has zero supporting artifact.

## Observed Facts
- `rg -i "highs|cplex|gurobi|cbc|scip" evidence/ reports/ benchmarks/` → zero matches.
- Repo-wide matches (excluding `docs/references.md`) are prose or guard code only: `docs/sih26119_problem_statement.md:44`, `docs/history.md:12`, `docs/architecture.md:10-11`, `docs/research_paper_references.md` (bibliography), and the *forbidden* lists in `scripts/check-sovereignty.py:19-34`.
- All 43 tests (`CMakeLists.txt:166-224`) invoke only `markov-cero-solve`, other markov-cero binaries, or Python runners over markov-cero output.
- All recorded comparisons are internal: simplex vs CPU PDLP vs GPU PDLP (`evidence/benchmarks/crossover_study.csv:1` header) or `simplex_obj` vs `pdlp_obj` (`evidence/benchmarks/phase4.json`).
- Independently recorded as PS-GAP-03 / "Comparison vs established solver … NONE" at `docs/audit/00-ground-truth.md:188,255`.

## Impact (Inference)
- R16 ("compared against at least one established commercial or open-source solver") is unmet; R20's "faster than weaker implementations" has no referent.
- Self-comparison cannot detect that both engines share the same modelling or numeric bugs.

## Related
- [[missing-hardware-metadata-in-evidence]] · [[testing-gaps]] · [[no-external-solver-dependency]]
