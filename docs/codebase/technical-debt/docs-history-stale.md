---
type: codebase-tech-debt
tags: [codebase, technical-debt, documentation]
severity: medium
status: resolved
verified_on: 2026-09-25
resolved_on: 2026-09-25
evidence:
  - "docs/history.md:34"
  - "CHANGELOG.md:3,54"
  - "README.md:5"
---

# Docs History Stale

> `docs/history.md` stops at milestone M5 (2026-09-18) while CHANGELOG/README describe Phases 5–6, and two doc pointers dangle.

## Observed Facts
- Last chronology in `docs/history.md` is milestones M0–M5 (`:9-37`) followed by "Phase 3" (`:39`) and "Phase 4" (`:45`), then a "Critical Audit Remediation" narrative (`:54-77`). No Phase 5 (GPU) or Phase 6 (QP/MIQP) entry exists (`rg -i "phase 5|phase 6|gpu|cuda|m6" docs/history.md` → zero matches), although both were released on 2026-09-21.
- `CHANGELOG.md` records "Phase 6: Convex QP & MIQP" under `0.5.2 — 2026-09-21` (`CHANGELOG.md:3-17`) and "Phase 5: GPU Acceleration" (`CHANGELOG.md:19-33`) — both absent from `docs/history.md`.
- `CHANGELOG.md:39` credits synchronization with `CAPABILITY-MATRIX`; `CAPABILITY-MATRIX*` does not exist at repo root (glob, no match).
- README CI badge targets `Saikrishna0817/markov-zip1/actions/workflows/ci.yml` (`README.md:5`), a different repo name than markov-cero.

## Impact (Inference)
- Two authoritative-sounding history documents disagree; a reader following `docs/history.md` would miss the entire GPU/QP work.
- Badge/link rot undermines the "CI is green" claim for outside evaluators.

## Resolution (2026-09-25)

`docs/history.md` **archived** (frontmatter `status: archived`, banner pointing to
`CHANGELOG.md`) per [[12-keep-remove-rebuild]] §8.2 — no longer presents itself as living
documentation, so the Phase 5/6 contradiction is defused. Two sub-items remain open and are
tracked elsewhere: missing `CAPABILITY-MATRIX` reference in `CHANGELOG.md:39`
([[changelog-gaps]]) and the README CI-badge repo-name mismatch ([[verify-md-dangling-paths]]
adjacent; part of RW-10 doc sync).

## Related
- [[changelog-gaps]] · [[verify-md-dangling-paths]] · [[clean-room-provenance]]
