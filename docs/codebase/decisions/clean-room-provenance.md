---
type: codebase-decision
tags: [codebase, decision, provenance]
status: verified
verified_on: 2026-09-25
evidence:
  - "PROVENANCE.md:1-9"
  - "docs/decisions/ADR-M0-00-clean-room-baseline.md:3"
  - "docs/history.md:9"
---

# Clean-Room Provenance

> Every line is derived from requirements, independently written math, and public bibliography — never from competitor code.

## Observed Facts
- `PROVENANCE.md:1-9` records: baseline "empty original repository created for markov-cero M0 on 2026-09-13"; "External solver source inspected during implementation: **none**"; admitted inputs are requirements, Notion-page math statements, public bibliographic metadata, and "sanitized behavior-level competitor observations"; excluded are "competitor code, pseudocode, tests, identifiers, constants, layouts, and control-flow recipes"; "Production external-solver path: prohibited and absent."
- Change discipline: "Each later change must state sources consulted, source exposure, derivation references, affected invariants, and reviewer status. Contamination triggers quarantine and clean reimplementation." (`PROVENANCE.md:9`).
- `docs/decisions/ADR-M0-00-clean-room-baseline.md:3` approves the original local repo (no remote, Apache-2.0) and "Competitor source and implementation-derived artifacts are excluded."
- `docs/history.md:9` records M0 "Outlawed all external solver libraries".

## Impact (Inference)
- This decision is the load-bearing answer to R10/R18; it is enforced mechanically by [[no-external-solver-dependency]] rather than by review alone.
- Because provenance is asserted in prose (and `VERIFY.md`'s manifest is absent), the claim rests on governance documents, not on a committed artifact chain — see [[verify-md-dangling-paths]].

## Related
- [[no-external-solver-dependency]] · [[verify-md-dangling-paths]] · [[numerical-policy-centralized]]
