---
type: codebase-audit
tags: [codebase, audit, integrity, wikilinks, backlinks]
status: verified
verified_on: 2026-09-25
---

# Vault Integrity — wikilink report

Checked with `python3 scripts/link_backlinks.py` (stdlib only, idempotent — a second run reports `notes patched: 0`).

## Note counts (340 notes)

| top folder | notes | sub-folders (note count) |
|---|---|---|
| `docs/research/` | 277 | papers 202 · concepts 16 · algorithms 17 · techniques 11 · metrics 8 · research-gaps 7 · limitations 6 · datasets 4 · architectures 3 · maps 2 · `Research MOC` 1 · engineering-decisions 0 |
| `docs/codebase/` | 46 | components 18 · technical-debt 11 · decisions 5 · APIs 3 · tests 3 · bugs 2 · audit 2 · data-flow 1 · `Codebase MOC` 1 · architecture 0 · modules 0 |
| `docs/` (root) | 10 | architecture, contracts, gpu, history, mathematics, references, consolidated_knowledge, research_paper_references, sih26119_problem_statement, threat-model |
| `docs/decisions/` | 4 | ADR-M0-00 … ADR-M0-03 |
| `docs/audit/` | 2 | 00-ground-truth, 07-current-architecture |
| `docs/governance/` | 1 | coding-standard |

## Wikilinks

- Raw `[[…]]` occurrences on disk: **3621** (332 distinct targets) — includes code fences and generated backlink bodies.
- Parsed by the generator (fences, inline code and `## Referenced By` bodies excluded): **2491**
  → **2447 resolved** · 13 unresolved · 31 ambiguous · 0 ignored path-qualified misses.
- Backlinks written: **71 of 72** `## Referenced By` sections populated; 1 left as placeholder (ambiguous).

## Unresolved stems (top offenders)

| stem | links | referenced from | nearest existing file |
|---|---|---|---|
| `Cut Pooling` | 10 | 10 cut-plan notes in `research/papers/` | none — concept note never written |
| `Adaptive Restart` | 1 | `research/papers/Lu-2025-cuPDLP-GPU-Implementation` | none — real gap |
| `SIH26119 Problem Statement` | 1 | `audit/00-ground-truth` | `docs/sih26119_problem_statement.md` (stem mismatch) |

These are reported, not papered over: `Cut Pooling` / `Adaptive Restart` need real notes;
`SIH26119 Problem Statement` needs a retarget, not a stub.

## Ambiguous stems (same basename in 2+ folders)

| stem | locations | bare links |
|---|---|---|
| `Presolve` | `research/concepts/Presolve.md`, `codebase/components/Presolve.md` | 31 |

`research/concepts/Presolve.md` keeps its placeholder `## Referenced By` until the links are
disambiguated; `codebase/components/Presolve.md` has no such section yet.

## Convention

- Qualify the **target**, not just the display text: `[[research/concepts/Presolve|Presolve (concept)]]`
  vs `[[codebase/components/Presolve|Presolve (component)]]`. The generator resolves `/`-qualified
  targets by path, so attribution is unambiguous and the skipped section populates on the next run.
- Whenever a stem is shared, the alias carries the folder: `[[stem|folder/stem]]`.
- Long-term fix: rename one note (`codebase/components/Presolve.md` → `Presolve-Component`).
- Never clear an unresolved count by inventing a stub note — create the real note or retarget the link.
