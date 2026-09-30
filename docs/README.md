# Documentation

Start with the [project README](../README.md) for the problem, architecture, demo, evaluation and team. This directory separates instructions that should work with the current checkout from dated project and research records.

## Current guides

| Guide | Use it for |
|---|---|
| [Quickstart](guides/QUICKSTART.md) | Build the solver, run a small model, and interpret a result |
| [Building](guides/BUILDING.md) | Build flags, C++ install, Python wheel, optional CUDA and benchmark tools |
| [Verification](guides/VERIFY.md) | Re-run checks, understand proof replay, and record fresh evidence |

## Project records

| Record | Use it for |
|---|---|
| [Status](project/STATUS.md) | Current capability boundaries and open acceptance gates |
| [Numerical contract](contracts/numerical-policy.md) | Binding tolerances, statuses, verification boundaries and assurance labels (contract v1) |
| [Resource limits contract](contracts/resource-limits.md) | Binding stop reasons, attribution rules and non-guarantees for cooperative resource stops (library, v1) |
| [Hosted limits contract](contracts/hosted-limits.md) | Kernel-enforced CPU, address-space, file, output and wall limits for each hosted solve child (service, v1) |
| [Provenance](project/PROVENANCE.md) | Source history, review limits and attributed inputs |
| [Original request](project/ORIGINAL_REQUEST.md) | Historical SIH scope and roadmap, not a completion certificate |
| [Changelog](../CHANGELOG.md) | Dated implementation and verification history |
| [Evidence index](../evidence/INDEX.md) | Frozen baselines, measured results and release-gate records |

## Research

[Research notes](research/README.md) are a dated literature and engineering workspace. Their embedded observations can describe earlier code states. Use the current status register and linked evidence before repeating a capability or performance claim. The notes are retained for traceability rather than presented as up-to-date implementation documentation.

The layout keeps source code in `src/`, public headers in `include/`, tests in `tests/`, optional CUDA work in `gpu/`, the visual app in `web/`, and results in `evidence/`. Build outputs and local dependency trees are ignored.
