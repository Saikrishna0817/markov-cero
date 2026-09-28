---
type: codebase-tech-debt
tags: [codebase, technical-debt, build]
severity: medium
status: resolved
verified_on: 2026-09-25
resolved_on: 2026-09-25
evidence:
  - "_deployment-phase2-build/"
  - "build/CMakeFiles/markov_cero_core.dir/src/milp/cuts.cpp.o"
  - ".gitignore:21-22"
  - "docs/audit/00-ground-truth.md:197"
---

# Stale Build Artifacts

> Old build trees on disk still contain objects for deleted sources and binaries for removed targets.

## Observed Facts
- `src/milp/cuts.cpp` does not exist (`ls src/milp/`: `cut_pool.cpp`, `gomory.cpp`, `mir.cpp`, …), yet `build/CMakeFiles/markov_cero_core.dir/src/milp/cuts.cpp.o` and `_deployment-phase2-build/CMakeFiles/markov_cero_core.dir/src/milp/cuts.cpp.o` are present on disk.
- `_deployment-phase2-build/` contains executables with no counterpart in the current `CMakeLists.txt` `add_executable` list (`CMakeLists.txt:97-164,250,254`): `foundation_test`, `m1_test`, `m1_edge_test`, `m1_property_test`, `m2_test`, `m3_test`, `m3_property_test`, `m4_test`, `m4_property_test`, `m5_test`, `m5_audit_regression_test`, `m5_property_test`.
- No `*.o`/`*.a`/`*.so` is tracked: `git ls-files "*.o" "*.a" "*.so"` → empty; `.gitignore:21-22` ignores them.
- `_deployment-phase2-build/` also holds a nested `reports/` and `evidence/` output tree that duplicates repo-root outputs.

## Impact (Inference)
- Stale `.o` files can be relinked by an incremental Make build without recompiling the deleted source, producing binaries that do not reflect `src/`.
- Disk inventory of ~1 GB-class scratch trees complicates "what is the shipped artifact" audits.

## Resolution (2026-09-25)

`build/` and `_deployment-phase2-build/` (with all other `_m5-*`/`_verify-*` trees) deleted
per [[12-keep-remove-rebuild]] §8.2; `build/` reconfigured and rebuilt from scratch, then
re-verified with the full CTest suite. No stale objects or deleted-target binaries remain on
disk. Root-cause fix for the duplicate: single build tree at `build/` only.

## Related
- [[scratch-dirs-in-repo]] · [[dead-fuzz-target]] · [[duplicate-benchmark-runners]]
