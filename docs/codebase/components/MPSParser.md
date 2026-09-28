---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/io/mps.cpp:96"
  - "include/markov_cero/io/mps.hpp:25"
verified_on: 2026-09-25
---

# MPSParser

> Fixed/free-form MPS reader producing `model::Model`, with quadratic sections and hard resource limits.

## Responsibility
- Parse NAME/OBJSENSE/ROWS/COLUMNS/RHS/RANGES/BOUNDS/QUADOBJ/QMATRIX streams into the internal model, rejecting malformed input with line-numbered errors.

## Implementation Facts (Observed)
- Entry points `parse_mps(std::istream&, MpsLimits = {})` and `parse_mps_string(std::string_view, MpsLimits = {})` (include/markov_cero/io/mps.hpp:25-33); implementation src/io/mps.cpp:96.
- Recognized sections: `NAME, OBJSENSE, OBJNAME, ROWS, COLUMNS, RHS, RANGES, BOUNDS, QUADOBJ, QMATRIX, ENDATA` (src/io/mps.cpp:84-86); unknown sections and records after ENDATA are errors (src/io/mps.cpp:176-180).
- Default limits: 16 MiB input, 1,000,000 lines/rows/columns, 20,000,000 nonzeros, 255-byte names (include/markov_cero/io/mps.hpp:12-17); exceeding them throws `std::length_error`-class resource failures (src/io/mps.cpp:139-164).
- `MpsError` carries the offending line number (include/markov_cero/io/mps.hpp:19-24, src/io/mps.cpp:91-94).
- Integer variables come from `INTORG`/`INTEND` MARKER records with nesting/imbalance checks (src/io/mps.cpp:251-262).
- `OBJSENSE` accepts one value and switches minimize/maximize (src/io/mps.cpp:189-221); `OBJNAME` selects the objective row (src/io/mps.cpp:191, 228).
- `QUADOBJ`/`QMATRIX` records fill `quad_entries` and set `has_quadratic_objective = true` (src/io/mps.cpp:203-205, 370, 444).
- Numerical tokens must be finite or parse as `MpsError "invalid numeric token"` (src/io/mps.cpp:77-80).

## Dependencies
- [[Canonicalizer]] (downstream consumer of `model::Model`)

## Used By
- [[CLI-MarkovCeroSolve]] (apps/markov_cero_solve.cpp:71), [[CLI-InfoAndMpsInspect]] (apps/markov_cero_mps_inspect.cpp:40), [[Solve-Pipeline]]

## Research Justification
- (no dedicated research note; MPS format reference in docs/references.md — UNVERIFIED content)

## Open Questions / Risks
- RANGES semantics are parsed into the model (row lower/upper) but their interaction with [[Presolve]] singleton/empty reductions is UNVERIFIED.
- Multiple rim vectors are explicitly unsupported (src/io/mps.cpp:303).
