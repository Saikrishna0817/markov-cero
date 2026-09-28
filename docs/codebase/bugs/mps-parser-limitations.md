---
type: codebase-bug
tags: [codebase, bug, parser, io]
severity: medium
status: verified
verified_on: 2026-09-25
evidence:
  - "src/io/mps.cpp:180"
  - "src/io/mps.cpp:238,303,308,332,364"
  - "tests/mps_parser_test.cpp:32"
---

# MPS Parser Limitations

> The free-format parser accepts a narrow MPS subset and hard-fails (`MpsError`) on several standard sections and records.

## Observed Facts
- Supported sections only: `NAME`, `OBJSENSE`, `OBJNAME`, `ROWS`, `COLUMNS`, `RHS`, `RANGES`, `BOUNDS`, `QUADOBJ`, `QMATRIX` (`src/io/mps.cpp:183-206`); any other section header sets `saw_end`, and a record in an unknown section throws `"unknown section: "` (`src/io/mps.cpp:180`) or `"record outside a supported section"` (`:377`).
- `QSECTION` is rejected by design — `tests/mps_parser_test.cpp:32-33` asserts that `parse_mps_string(... "QSECTION\nENDATA")` throws `MpsError`.
- Unsupported within supported sections: multiple rim vectors per RHS/RANGES block (`src/io/mps.cpp:303` "multiple rim vectors are unsupported"), objective-row RHS/RANGES values (`:308`), multiple bound vectors (`:332`), row types other than `N/E/L/G` (`:238`), bound types other than `LO/UP/FX/FR/MI/PL/BV/LI/UI` (`:364` "unsupported bound type").
- Structural limits enforced by exceptions: row/column/nonzero limits (`:140,243,284`), name/ASCII checks (`:121,126`), content after `ENDATA` (`:176`), missing `ENDATA` (`:380`), missing `INTEND` (`:382`), non-finite numeric tokens (`:80`).
- No `SOS`/`SOSTRNGS`, `CONES`, `QSECTION`, `OBJALIAS`, or `EXTRASECTIONS` handling appears anywhere in `src/io/mps.cpp` (456 lines, `rg` no matches).

## Impact (Inference)
- Real-world MIPLIB/Netlib files using SOS sections, multiple RHS vectors, or QSECTION quadratic blocks will be rejected outright rather than partially loaded — limiting R15/R19 dataset coverage and MIQP input paths.

## Related
- [[blend-numerical-failure]] · [[dead-fuzz-target]] · [[csc-sparse-storage]]
