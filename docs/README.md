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
| [Sparse LP path contract](contracts/sparse-lp-path.md) | Sparse-first LP dispatch rule, dimension envelope, shared fingerprints and the dense/sparse differential obligation (library, v1) |
| [Convex QP path contract](contracts/convex-qp.md) | Frozen Hessian convention, three-outcome PSD classification, KKT/verification tolerances, status mapping and CPU/GPU path disclosure (library, v1) |
| [MILP node bounds contract](contracts/milp-node-bounds.md) | Prune-reason/witness table, node lower-bound guards, branch-partition rules, cut obligations and proof/label semantics (library, v1) |
| [MIQP node bounds contract](contracts/miqp-node-bounds.md) | Supporting lower-bound derivation, fail-closed bound rules, node status mapping, original-quadratic incumbent verification and QP-leaf replay (library, v1) |
| [Local SQP and callback contract](contracts/nlp-local-sqp.md) | Callback validation at every evaluation, derivative diagnostic, SQP iteration semantics with the Armijo waiver, LocalStationary status rules and verified-feasible-incumbent handling (library, v2) |
| [Elastic restoration contract](contracts/nlp-restoration.md) | Primal-infeasible linearized QP handling: frozen failure corpus, exact penalty and acceptance rules, bounded inconclusive failures and paired benchmark obligation (library, v1) |
| [Restricted convex MINLP OA contract](contracts/minlp-oa.md) | Frozen structural scope and 512 screen, OA cut derivation with provenance and independent source replay, incumbent/master-bound certification, caps and counters, cases A–H test obligations (library, v1) |
| [Convex MINLP OA proof replay contract](contracts/minlp-proof-replay.md) | Versioned OA proof object and obligations O1–O12, independent master-tree replay, tier/certificate/status map with the oa_replayed label, shared proof budgets, attack/enumeration/CLI test obligations (library, v1) |
| [Hosted limits contract](contracts/hosted-limits.md) | Kernel-enforced CPU, address-space, file, output and wall limits for each hosted solve child (service, v1) |
| [Provenance](project/PROVENANCE.md) | Source history, review limits and attributed inputs |
| [Original request](project/ORIGINAL_REQUEST.md) | Historical SIH scope and roadmap, not a completion certificate |
| [Changelog](../CHANGELOG.md) | Dated implementation and verification history |
| [Evidence index](../evidence/INDEX.md) | Frozen baselines, measured results and release-gate records |

## Research

[Research notes](research/README.md) are a dated literature and engineering workspace. Their embedded observations can describe earlier code states. Use the current status register and linked evidence before repeating a capability or performance claim. The notes are retained for traceability rather than presented as up-to-date implementation documentation.

The layout keeps source code in `src/`, public headers in `include/`, tests in `tests/`, optional CUDA work in `gpu/`, the visual app in `web/`, and results in `evidence/`. Build outputs and local dependency trees are ignored.
