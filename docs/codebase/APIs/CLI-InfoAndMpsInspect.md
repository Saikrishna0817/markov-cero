---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "apps/markov_cero_info.cpp:3"
  - "apps/markov_cero_mps_inspect.cpp:29"
verified_on: 2026-09-25
---

# CLI-InfoAndMpsInspect

> Two small utilities: `markov-cero-info` (build identity) and `markov-cero-mps-inspect` (model summary JSON).

## Responsibility
- Report version/milestone without arguments, and parse-only model statistics without solving.

## Implementation Facts (Observed)
- Targets declared as `markov-cero-info` and `markov-cero-mps-inspect` linking `markov_cero_core` (CMakeLists.txt:97-100).
- **markov-cero-info** (`apps/markov_cero_info.cpp`, 8 lines): takes no arguments, prints `markov-cero <version> milestone <milestone> (sparse basis linear algebra and update substrate)` from `markov_cero::foundation::version()`/`milestone()` (apps/markov_cero_info.cpp:3-6); returns 0.
- **markov-cero-mps-inspect** (`apps/markov_cero_mps_inspect.cpp`): requires exactly one argument else prints `usage: markov-cero-mps-inspect MODEL.mps` and exits 2 (apps/markov_cero_mps_inspect.cpp:30-33); unopenable file → exit 3 (apps/markov_cero_mps_inspect.cpp:34-38); parse exception → stderr message and exit 4 (apps/markov_cero_mps_inspect.cpp:47-50).
- Success emits one JSON line: `{"name":..., "sense":..., "rows":..., "columns":..., "nonzeros":...}` built from `io::parse_mps` output (apps/markov_cero_mps_inspect.cpp:40-45).
- Local `escape()` helper mirrors the JSON escaping used by [[Solution-JSON-Writer]] but is duplicated in this file (apps/markov_cero_mps_inspect.cpp:6-28).
- No options/flags beyond the positional model path (no `CliOptions` include; only `markov_cero/io/mps.hpp`).

## Dependencies
- [[MPSParser]] (`io::parse_mps`), [[Solution-JSON-Writer]] (conventions only — duplicated code, not shared)

## Used By
- (standalone tools; referenced by docs/scripts — UNVERIFIED which)

## Research Justification
- (no research note; diagnostics surface)

## Open Questions / Risks
- Exit codes (2/3/4) differ from [[CLI-MarkovCeroSolve]]'s status mapping — Inference: tooling must special-case these binaries.
- `sense` string comes from `model::to_string(objective_sense)` (apps/markov_cero_mps_inspect.cpp:42), consistent with include/markov_cero/model/model.hpp:72.
