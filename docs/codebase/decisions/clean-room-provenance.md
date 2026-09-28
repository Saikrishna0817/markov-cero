---
type: codebase-decision
tags: [codebase, decision, provenance]
status: disputed
verified_on: 2026-09-28
evidence:
  - "PROVENANCE.md:1-9"
  - "docs/decisions/ADR-M0-00-clean-room-baseline.md:3"
  - "docs/history.md:9"
---

# Historical Provenance Claim — Review Required

> The prior unqualified clean-room statement is withdrawn pending independent trace review.

## Observed Facts
- Historical records date the M0 baseline to 2026-09-13 and say no external solver implementation source was consulted during that coding period. This record is not independently corroborated.
- `docs/audit/19-competitive-landscape.md` records 27 peer solver repositories cloned and inspected locally for line-level verification on 2026-09-25. This is later source-level exposure and conflicts with the former project-wide wording that competitor code was excluded.
- The purpose recorded for the 2026-09-25 inspection was retrospective competitive analysis. Whether it influenced implementation has not been independently reviewed.
- Production external-solver linkage is prohibited and absent. Dependency isolation does not establish clean-room provenance.
- `docs/decisions/ADR-M0-00-clean-room-baseline.md:3` approves the original local repo (no remote, Apache-2.0) and "Competitor source and implementation-derived artifacts are excluded."
- `docs/history.md:9` records M0 "Outlawed all external solver libraries".

## Current determination

Clean-room status is **unverified**. Do not make an unqualified clean-room claim until an independent reviewer checks exposure dates, relevant history/diffs and algorithm-level similarities. If affected components are identified, quarantine and reimplement them under the approved policy.

## Impact (Inference)
- Dependency isolation supports part of R10/R18, but source independence remains a separate review question.
- Because provenance depends on historical prose and the exposure review is pending, it is not yet a release-grade evidence chain.

## Related
- [[no-external-solver-dependency]] · [[verify-md-dangling-paths]] · [[numerical-policy-centralized]]
