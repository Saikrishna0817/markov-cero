---
type: codebase-tech-debt
tags: [codebase, technical-debt, repository-hygiene]
severity: low
status: resolved
verified_on: 2026-09-25
resolved_on: 2026-09-25
evidence:
  - ".gitignore:2-6"
  - "_deployment-phase2-build/"
  - "_m5-deploy-clangxx/"
---

# Scratch Dirs In Repo

> Eight generated build/verify trees sit at the repo root; all are gitignored and none are tracked.

## Observed Facts
- On-disk scratch dirs at root: `_deployment-phase2-build`, `_m5-deploy-clangxx`, `_m5-deploy-clangxx-sanitize`, `_m5-deploy-fuzz`, `_m5-deploy-gxx`, `_m5-deploy-gxx-sanitize`, `_verify-clang`, `_verify-gcc` (plus `build/`).
- All are ignored: `git check-ignore -v` reports `.gitignore:3:_deployment-*/` for `_deployment-phase2-build`, `.gitignore:2:_m5-*/` for `_m5-deploy-*`, `.gitignore:6:_verify-*/` for `_verify-*`, `.gitignore:4:build/` for `build`.
- None are tracked: `git ls-files | grep -E "^_m5|^_verify|^_deployment"` → empty; `git status --porcelain` shows only untracked `docs/audit/`, `docs/consolidated_knowledge.md`, `docs/research_paper_references.md`, `docs/sih26119_problem_statement.md`.
- Gitignore also covers `reports/`, `data/scale_study/`, `.licenses/`, `scripts/__pycache__/` (`.gitignore:52,51,53,41`).
- Note: `docs/audit/00-ground-truth.md:199` claims these dirs are "none gitignored-clean"; that claim is contradicted by the `git check-ignore` results above.

## Impact (Inference)
- Repo-cloning size and audit surface stay clean, but a working checkout carries several redundant full build trees (each with its own CMake cache), risking confusion about which tree produced which evidence.

## Resolution (2026-09-25)

All eight scratch trees deleted from the working copy per [[12-keep-remove-rebuild]] §8.2
(`_m5-*`, `_deployment-*`, `_verify-*`; ~99 MB recovered). `build/` was also removed and
rebuilt fresh — see [[stale-build-artifacts]]. Side benefit: `scripts/*` binary-candidate
lists (which preferred `_deployment-phase2-build/` over `build/`) can no longer resolve a
stale binary first. `.gitignore` entries were left in place so the dirs cannot be
re-committed by accident.

## Related
- [[stale-build-artifacts]] · [[dead-fuzz-target]] · [[duplicate-benchmark-runners]]
