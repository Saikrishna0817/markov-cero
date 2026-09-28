---
type: codebase-tech-debt
tags: [codebase, technical-debt, documentation]
severity: low
status: verified
verified_on: 2026-09-25
evidence:
  - "CHANGELOG.md:3,54,62,71,78"
  - "CHANGELOG.md:17"
  - "evidence/local-verification-report.txt:236"
  - "git log --oneline"
---

# Changelog Gaps

> The changelog skips versions 0.3.0–0.5.0, omits the three newest commits, and its pass-rate claim outruns its evidence.

## Observed Facts
- Version headings present: `0.5.2`, `0.5.1`, `0.0.1`, `0.1.0`, `0.2.0` (`CHANGELOG.md:3,54,62,71,78`). No `0.3.0`, `0.4.0`, or `0.5.0` section exists.
- Newest repo commits are `d4c2743`, `255bf62`, `7ca734f`, `5fe1472`, `e978d41` (`git log --oneline`); `rg "7ca734f|5fe1472|e978d41|d4c2743" CHANGELOG.md` → zero matches, so the CI/sanitizer/sovereignty fixes are unreleased in the changelog.
- `CHANGELOG.md:17` claims "Expanded automated test suite to 43 CTest targets with 100% pass rate"; the only recorded run in-repo reports "100% tests passed out of 41" (`evidence/local-verification-report.txt:236`, 2026-09-21).
- `VERSION` file content is `0.5.2`, matching the newest heading.

## Impact (Inference)
- Release notes cannot be used to reconstruct what changed between 0.2.0 and 0.5.1, or to attribute the post-0.5.2 CI fixes.
- The 43/100% claim is UNVERIFIED by any committed artifact.

## Related
- [[docs-history-stale]] · [[overview]] · [[verify-md-dangling-paths]]
