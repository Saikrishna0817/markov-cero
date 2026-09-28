---
type: codebase-tech-debt
tags: [codebase, technical-debt, documentation]
severity: low
status: verified
verified_on: 2026-09-25
evidence:
  - "VERIFY.md:3"
  - "scripts/verify-release.sh:5-6"
---

# VERIFY.md Dangling Paths

> `VERIFY.md` instructs reviewers to regenerate a provenance manifest that the repo does not contain and the release script does not touch.

## Observed Facts
- `VERIFY.md:3` states: "Any source change requires regenerating `provenance/source-manifest.sha256`, committing, and repackaging."
- No `provenance/` directory exists at repo root (`ls provenance` → No such file or directory); `git ls-files provenance` → empty.
- `scripts/verify-release.sh` writes only `evidence/local-verification-report.txt` and `evidence/environment-local.json` (`scripts/verify-release.sh:5-6`); `rg "provenance|manifest|sha256"` over that script returns no matches.
- The two evidence files it writes are themselves gitignored (`.gitignore:17-18`).

## Impact (Inference)
- The documented release-verification procedure cannot be executed as written; the manifest-based integrity check it promises is absent.
- Since its outputs are gitignored, no reviewer can diff a verification run against a committed baseline.

## Related
- [[docs-history-stale]] · [[changelog-gaps]] · [[clean-room-provenance]]
