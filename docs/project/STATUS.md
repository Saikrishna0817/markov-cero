# Capability and release status — v0.5.2

**As of 2026-09-29.** This is the current entry point for implementation claims. The project is a research prototype for Smart India Hackathon problem statement SIH26119. A feature marked *implemented* has code and named checks; it is not a claim of general solver competitiveness or refinery operational readiness. Dated test counts and benchmark results in [evidence](../../evidence/INDEX.md) are tied to their recorded source and binary, not automatically to this checkout.

## Release decision

**The production and refinery gates remain open.** The [gate status record](../../evidence/gate-status-20260929.json) and [defect closure register](../../evidence/defect-closure-register.csv) name the remaining work. The main boundaries are complete deadline/allocation coverage, independent provenance review, broader numerical and benchmark coverage, support ownership, and refinery engineer sign-off with a shadow trial. The license conflict is resolved: an owner decision (D18, 2026-09-29) set the Python package metadata to `Apache-2.0` to match the checked-in license, with a CI coherence check.

Blueprint task BASE-01 is complete: a clean Release build of revision `aa6f35e` built warning-free with `-Werror`, passed 92/92 CTest and 22/22 Python binding tests, and an external consumer linked the installed CMake package and replayed an independent MIP proof. Commands, hashes and raw logs are in the [baseline manifest](../../evidence/baseline-manifest-20260929.json); that manifest makes no speed, GPU, ML, benchmark-breadth or refinery-readiness claim.

## Implemented paths and their limits

| Area | Current state | Important boundary |
|---|---|---|
| Input and model | Free-format MPS and LP parsers, integer markers, quadratic MPS sections, sparse model representation, classification and input caps | Older benchmark results before parser corrections need regeneration against a pinned current binary. |
| Transforms | Sparse canonicalization, reversible presolve/postsolve and Ruiz scaling | Original-model feasibility must still be checked after transforms. |
| LP | Primal revised simplex, dual warm simplex, interior-point and matrix-free PDLP | Dense and factor fill limits, numerical failure cases and hard-instance coverage remain. |
| Sparse algebra | Sparse basis LU, fill-reducing ordering, eta updates, refinement and cooperative deadline polling | Deadline checks are cooperative; memory accounting does not cover every allocation. |
| MILP | Branch-and-cut, GMI/MIR cuts, heuristics, strong branching, bounded node queue and parallel search | Hard cases can time out or stop at limits; a retained incumbent is not a global proof. |
| Convex QP/MIQP | ADMM and KKT work, MIQP relaxations, residual verification | Convexity assumptions and numerical tolerances apply; general nonconvex QP is not claimed. |
| NLP/MINLP | SQP/L-BFGS local NLP path and restricted convex/quadratic outer approximation | Local NLP outcomes are not global proofs. Arbitrary callback nonlinear MINLP is rejected or unsupported. |
| Independent checking | Original-space primal checks, canonical LP witnesses, QP KKT checks, bounded MILP/MIQP proof-tree replay, IIS analysis | Replay shares parser and numerical primitives; proof budget exhaustion keeps the global result unverified. OA/MINLP has no global proof export. |
| GPU | Optional CUDA PDLP kernels and partial QP P·x device path | RTX 2050 correctness was recorded, but no measured end-to-end GPU speed advantage; QP factorization/x-update remain CPU-side. |
| ML branching | Optional data logging and ONNX scorer path | No trained artifact has passed data split, runtime validation and solve-outcome promotion gates. |
| Interfaces | C++ API, CLI/JSON, Python bindings, visual web app and optional authenticated HTTP adapter | No versioned C ABI. The web presentation needs a separately configured solver API for live solves. |
| Refinery examples | Synthetic qualification cases and attributed public historical Fawley model | No approved MRPL plant data or engineer-owned operational validation. |

Code and test entry points are mapped in the [repository guide](../README.md); implementation history is in the [changelog](../../CHANGELOG.md). This table deliberately summarizes current scope without repeating every historical milestone.

## Result interpretation

The CLI reports a status, resolved engine, objective or bound, verification fields and diagnostics. A valid feasible incumbent can coexist with a time, node, memory, numerical or proof limit. `verified` is the overall result gate; `original_verified` and certificate fields describe narrower checks. For linear MILP and convex MIQP, a bounded cut-free tree can be exported and replayed by `markov-cero-verify-mip`; an incomplete or rejected tree cannot establish global optimality or infeasibility. See [verification](../guides/VERIFY.md) and [proof evidence](../../evidence/proof-guarantee-20260928.json).

`--time-limit`, `--memory-limit-bytes`, `--max-nodes` and `--max-queued-nodes` expose separate controls. Reaching the queued-node cap returns `ResourceLimit` and retains the omitted frontier's inherited lower bound. A byte budget covers instrumented charge points; it is not complete RSS or device-memory enforcement. [Resource evidence](../../evidence/resource-envelope-20260928.json) records the tested boundary.

## Recorded evaluations

These observations are historical run results. They should be rerun after a behavior-changing source change before being presented as a current-binary score.

| Evaluation | Recorded result | Interpretation |
|---|---|---|
| Five-solver W9, 23 cases, 15-second cap | markov-cero 17/23 optimal; HiGHS 17/23; GLPK 14/23; CBC 13/23; SCIP 18/23. No paired verified optimum disagreed. | Six markov-cero cases failed or timed out; the comparison does not establish broad competitiveness. [Report](../../evidence/comparison/current_glpk_pinned_20260928/full_comparison_report.md) |
| Pinned two-repeat HiGHS, 20 preregistered cases | Verified objective agreement 20/20 at both one and four markov-cero threads. Runtime ratios markov-cero/HiGHS: 15.61× and 12.76×. | The ratios favored HiGHS, and API/subprocess timing boundaries differed. [One thread](../../evidence/compare/current_final_threads1_20260928/report.md), [four threads](../../evidence/compare/current_final_threads4_20260928/report.md) |
| Local benchmark sweep, 15-second solver cap | 255 checked-in Netlib/MIPLIB/Mittelmann/QPLIB rows attempted; three exceeded parent watchdog. | Full upstream collections and 300-second study remain open. [Evidence index](../../evidence/INDEX.md) |
| RTX 2050 GPU PDLP repeat | Four generated cases verified in multiple runs; GPU was slower end to end than CPU PDLP. | No GPU benefit is established. [Host record](../../evidence/gpu_hardware_host_access_check_20260928.json) |
| Public Fawley qualification model | 29 rows, 36 columns; local markov-cero and development HiGHS oracle objective −2899.252790423 thousand historical USD/period. | Checks this generated model only; not a plant validation. [Case guide](../../examples/refinery/README.md) |

## What must happen next

1. Close the remaining critical/high defects and demonstrate deadline, memory and worker-failure behavior across engines, verifiers and device allocations.
2. Repeat correctness and performance tests against pinned current binaries on representative difficult models with aligned timing boundaries and explicit failures in the denominator.
3. Complete independent source/provenance review and resolve the `LICENSE` versus `pyproject.toml` metadata conflict before release publication.
4. Obtain refinery engineer approval of the formulation, units and data, then conduct an agreed shadow trial before operational claims.
5. Promote GPU or ML performance claims only after matched, repeatable, outcome-verified comparisons.

The [original request](ORIGINAL_REQUEST.md) is a historical scope record. The [evidence index](../../evidence/INDEX.md) is the starting point for underlying artifacts; the [project README](../../README.md) explains the system for readers new to optimization.
