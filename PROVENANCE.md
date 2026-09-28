# Clean-room provenance

- Baseline: empty original repository created for markov-cero M0 on 2026-09-13.
- Implementation role: independent clean-room session.
- External solver source inspected during implementation: **none**.
- Inputs admitted: approved project requirements, mathematical statements independently written in the governing Notion pages, public bibliographic metadata, and sanitized behavior-level competitor observations.
- Excluded: competitor code, pseudocode, tests, identifiers, constants, layouts, and control-flow recipes.
- Production external-solver path: prohibited and absent.

Each later change must state sources consulted, source exposure, derivation references, affected invariants, and reviewer status. Contamination triggers quarantine and clean reimplementation.

## Public refinery benchmark (2026-09-28)

The numeric input in `data/refinery/fawley_public.json` was transcribed from
[GAMS FAWLEY model 65](https://www.gams.com/latest/gamslib_ml/libhtml/gamslib_fawley.html),
which cites Palmer, *A Model Management Framework for Mathematical Programming* (1984).
The published modeling formulation was read; no optimization solver implementation
was read or incorporated. The Python generator was independently implemented from
these data and balance equations. This is a historical illustrative model, not
MRPL data or an engineer-approved operating model. Its RON and viscosity rules are
approximate; lead cost and fuel-equivalent transfers retain historical semantics.
Fuel-equivalent quantities must not be interpreted as conserved physical mass.

Input units and source metadata are embedded in the JSON; the generated MPS embeds
its input SHA-256. The dataset is attributed external material, not a claim of
ownership or a grant of rights by this project. Redistribution rights and plant
engineering approval remain review gates. HiGHS is used only as a development
comparison oracle and is not linked to the production solver.
