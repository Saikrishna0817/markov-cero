# Industry-readiness implementation plan

**Project:** markov-cero  
**Baseline reviewed:** 2026-09-28 source tree  
**Purpose:** close correctness, numerical, engineering, repository, SIH-evidence and refinery-workflow gaps before describing this as industry grade.

This is an implementation plan, not a release certification. The current project is a research solver prototype. A feature being present in source, or a local test suite being green, does not establish broad robustness, commercial-solver parity, or refinery readiness.

## Implementation checkpoint — 2026-09-28

Implemented: all five former numerical/domain failures pass; sparse revised-simplex columns/pricing/basis assembly and dual refinement; QP equilibration with original-unit output; every maintained code/build file at most 300 physical lines; 227 optional data files migrated to hash-checked explicit restoration; cut-free numerical MILP/convex-MIQP proof replay and standalone checker; supported row-conflict CLI; bounded LP/ONNX/proof inputs; caller-owned ML options; corrected packaging relink and matched process timing. Invalid bundled ML weights were withdrawn, not promoted.

Validation: Debug CPU 80/80 CTests; fresh optional-ML Release 80/80; installed wheel 16/16; installed CMake consumer and sovereignty guard pass. Exact hashes and scoped outcomes are in `evidence/readiness-checkpoint.json`. The original checkpoint is preserved in `evidence/readiness-baseline-20260928.json`. Production planning meets its requested 5% gap: the default five-second proof stage expires, while an extended 45-second generation/10-second replay run accepts its 157-node numerical proof.

The deduplicated [closure register](../../evidence/defect-closure-register.csv) reconciles the 36 seed items: **29/32 actionable code defects closed (90.625%)**, three open engineering defects (IR-19/20/21), and four separate capability/release gates (IR-28/33/34/35). The four gates are not counted as fixes. This implementation tally is scoped to known baseline defects and has not received an external release audit. The 100% critical/high gate is unmet. Full immutable node views, complete deadline coverage, solve-wide memory budgeting, supply-chain/hosted validation and refinery sign-off remain visible blockers; the project is still a research prototype.

## 1. Measurable completion target

Before implementation starts, merge the findings in the current codebase audit, the SIH findings register, the full implementation audit, and the supplied review into one deduplicated defect register. Preserve each finding's source, severity, affected module, owner, disposition, and closure evidence. Classify a finding as resolved only after its regression or acceptance evidence passes; changing its wording or moving it to “deferred” does not count as closure.

The release target is:

- Resolve **100% of critical and high-severity defects in shipped paths**, including correctness, certificates, parsers, deadlines, security, packaging and CI integrity.
- Resolve **at least 90% of all deduplicated, actionable correctness, numerical, reliability, and security defects** in the baseline register.
- Close every remaining open issue with a precise, visible limitation and a release block if it affects the advertised use case.
- Require every accepted result status to have an independently checkable meaning; no result may be called verified when only the algorithm's own stopping rule passed.
- Keep experimental engines out of the default production profile until their own acceptance gates pass.

The denominator is the count of unique, actionable defect findings at baseline, not the count of historical notes or feature requests. Pure roadmap work such as an optional GPU kernel is classified separately from defects. The final report must show the numerator, denominator, severity coverage, and evidence links. Do not promise 90% closure before that register exists.

## 2. Release-blocking findings already verified

### P0 — MPS integer-marker bound override

In **src/io/mps.cpp**, columns first seen inside an INTORG block are assigned an upper bound of 1 in two code paths. That default is permitted by major MPS readers; [Gurobi documents a 0–1 marker default with later BOUNDS overrides](https://docs.gurobi.com/projects/optimizer/en/current/reference/fileformats/modelformats.html) and [CPLEX also describes a 0–1 default within markers](https://www.ibm.com/docs/en/icos/22.1.1?topic=extensions-integer-variables-in-mps-files). The bug is the interaction with explicit bounds: an LI record changes the lower bound but leaves the marker's upper bound at 1. The bundled gen-ip002 and gen-ip054 files contain INTORG markers followed by LI records; a read-only HiGHS parse of those inputs reports integer columns with upper bound +∞. The current source therefore describes a different domain for those inputs. This observation establishes a parser mismatch, not a fresh solve result.

**Fix and acceptance:**

- Preserve the documented 0–1 default for marker-only columns in the chosen MPS dialect. Track whether each endpoint is a marker default or an explicit bound, then apply LI, UI, LO, UP, PL and other supported BOUNDS records according to that dialect. For the bundled LI cases, the integer upper bound must match the reference parser's +∞ domain.
- Make LI and UI set integer type and the specified endpoint while applying the required implicit endpoint reset, if any. Apply bounds in legal order, validate contradictions, and document whether contradictory bounds produce a valid infeasible model or a parse error.
- Cover integer columns with no explicit bounds, LI equal to 0 and greater than 1, UI only, LO plus UI, UP plus LI, PL, BV, marker transitions, columns first encountered in BOUNDS, and both valid and contradictory ranges.
- Add small fixtures whose exact feasible integer domains and optima can be enumerated. Differentially compare parser interpretation and solve outcomes against a separately installed reference parser/solver in the test environment; do not use that solver at runtime.
- Re-run affected MIPLIB instances, replace stale CSV rows with hash-pinned fresh outcomes, and keep prior outcomes as history rather than silently overwriting them.
- Gate any MPS MILP result on an input-semantics regression suite. Until this is closed, do not rely on affected general-integer MPS results.

### P0 — One certificate contract across all engines

The result path must distinguish algorithm termination from proof. A common independent verifier can own primal feasibility and LP/QP certificate checks. A MIP search proof needs additional validated tree accounting; a primal vector and reported global bound alone cannot establish MIP optimality or infeasibility.

- Recompute primal feasibility in original model space, variable-bound feasibility, integrality, objective value, and objective consistency from the returned vector.
- For LP, check dual feasibility, row and variable-bound multiplier signs, and primal-dual objective gap. For QP, check stationarity, primal feasibility, multiplier signs for the active bound side, and side-specific complementarity. For nonlinear models, report local KKT conditions separately; never label local KKT convergence as global optimality.
- Replace the global max-RHS tolerance in **src/milp/node_lp.cpp**. For row i use a local backward-error denominator such as |b_i| + Σ_j |a_ij| |x_j| plus an absolute floor; use analogous local scales for reduced costs and bounds. Also enforce an absolute residual limit where engineering units require one. Check variable bounds and all non-finite values. A large RHS in one row must not loosen checks for every other row.
- Recompute the objective from the original objective vector and returned primal point. Check the reported objective and bound independently.
- Give LP optimality a documented residual and gap gate. A feasible primal and feasible dual without a sufficiently closed primal-dual gap is not an optimality certificate.
- Give MIP results explicit fields for incumbent, global bound, relative and absolute gaps, unresolved nodes, termination reason, and proof status. If the configured nonzero relative gap is reached, return **gap satisfied** (or an equivalent distinct status), not numerically certified optimality. Define gap denominators for zero and near-zero objectives and avoid an absolute-value formula that can misrepresent bound ordering. Remove the magic large-bound substitution in **src/milp/milp_solver.cpp** that replaces any |best_bound| > 1e15 with the incumbent objective, which can fabricate a zero gap; retain unknown infinity and validate bound direction.
- Audit MILP/MIQP proof accounting separately: certify every node relaxation used for pruning; verify presolve, branching-domain and bound-propagation transformations; validate Gomory/MIR and local cut validity and scope; and account for every child partition, active node and exhausted node. Produce a replayable search ledger or proof log if externally checkable global MIP claims are required. Until then, distinguish an independently checked incumbent from a solver-trusted global bound/tree conclusion.
- Define separate statuses for **optimal certified within documented numerical tolerances**, gap satisfied at the requested nonzero MIP gap, feasible incumbent, approximate feasible point, infeasible with a supported certificate, unbounded with a supported ray, resource limit, numerical failure, unsupported, and invalid input. Reserve “exact” for an exact-arithmetic proof, which is outside this plan.
- For QP/MIQP infeasibility and unboundedness, implement and check appropriate solver-specific certificates if supported. Otherwise return an inconclusive/numerical status rather than claiming a proof. MIQP global pruning also requires certified convex QP node relaxations and a sound PSD classification.
- Expose the actual certificate type, residuals, tolerances, model hash, and solver build identity in C++ results and JSON.
- Add adversarial tests for wrong dual signs, large unrelated RHS values, bound violations, non-finite witnesses, incorrect objective telemetry, nonzero MIP gap, and resource limits during proof.

A verifier that consumes the same incorrectly parsed model is not an independent check of file semantics. Parser conformance must be tested separately against fixtures and an external test oracle; the runtime certificate must verify the parsed model faithfully.

### P0 — Refinery model must conserve material and enforce quality

The current illustrative refinery LP permits FCC product output without an FCC feed balance, and stores quality data and sulfur limits that are not fully enforced. A feasible optimum is not an operationally valid refinery plan if a unit manufactures un-fed product or ignores a product specification.

- Add an explicit FCC feed stream and connect it to available feed components, unit capacity, yields, conversion losses, coproducts, and any required operating mode.
- Close all component and total mass balances. The current example uses kbpd volumetric flows, so require stream density and a declared temperature/reference basis to convert volumes to conserved mass. Model volume shrinkage/expansion and yield bases separately. Represent losses, fuel gas, LPG, residue, and other outlets explicitly where the intended process model requires them.
- Encode sulfur, octane/RON, cetane, API/density, vapor pressure, and other configured specifications using the correct unit conventions and blending equations. Sulfur wt% and ppm require mass-fraction conversion; derive blend API gravity from blended density, not a direct average of API values. Distinguish linear volume-weighted properties from nonlinear or non-additive quality variables.
- Add units to every input, variable, constraint, objective coefficient, and result. Validate ranges, missing data, and unit conversions at model construction.
- Mark the current small refinery case as synthetic solver qualification data. Do not present it as MRPL process data or production planning.
- Make pooling SLP report original nonlinear residuals, fixed-point residual, iteration count, and termination reason. A converged fixed point is a local heuristic result, not a global certificate. Add multiple starts or a suitable global method only if the use case needs a global guarantee.
- Obtain refinery engineer review of equations and assumptions, then compare against an independently prepared, anonymized case and historical plan. Record model owner, data provenance, units, tolerances, and approval.
- No operating decision may depend on the demo until a domain engineer signs off the formulation and result interpretation.

## 3. Ordered workstreams

### Workstream A — Freeze the baseline and establish evidence

1. Freeze a clean source revision and record compiler, build flags, enabled features, dependency versions, platform, model hashes, executable hashes, and test results. Do not attribute results from ignored build directories to new source unless hashes match.
2. Reconcile **STATUS.md**, current findings, benchmark CSV/JSON reports, README, CI and release scripts. Separate code present, test-covered, benchmarked, and accepted capabilities.
3. For every defect, record a reproducible fixture or named input, current behavior, expected behavior, severity, fix, regression gate, and evidence artifact.
4. Preserve pre-fix evidence with its original revision and mark it superseded after corrected runs. Never edit a historical result into a passing result.
5. Confirm parser, certificate, and refinery P0 defects are release blockers for their respective advertised paths.

**Exit gate:** one deduplicated register; every public claim points to current evidence; every baseline artifact is tied to exact source and executable hashes.

### Workstream B — Mathematical and numerical correctness

1. Fix MPS integer bounds as described above. Complete a semantics matrix for objective sense and constants, row senses, ranged rows, free variables, all supported bound codes, integer markers, binary variables, duplicate entries, duplicate names, and unsupported sections. Reject unsupported syntax explicitly rather than silently changing meaning.
2. Test model transformation round trips: original → canonical → postsolve → original. Verify objective sign, row sense, bounds, names, dual signs, and sparse coefficients using generated small models and hand-computable fixtures.
3. Centralize residual scaling and tolerance policy. Report absolute residual, scaled residual, scale definition, and configured tolerance for each result. Never use one model-wide maximum to relax small rows or variables.
4. Use one engine-independent result gate for primal, dual, bound, objective, integrality, and LP/QP certificate validation. Audit MIP tree proof independently as specified above. Engine-specific convergence is evidence for a candidate only.
5. Rename or replace the current EXPAND-like anti-degeneracy rule in **src/lp/reference/revised_simplex.cpp**. The current perturbation changes ratio selection without implementing the named algorithm's proof. Keep a proven Bland/lexicographic fallback for termination; experimental perturbations must not be described as a termination guarantee.
   Diagnose the disabled **regression_simplex_scale200_pricing** case in CMakeLists.txt, restore or replace its fixture, fix the witness/pricing failure, and enable it in the default CI gate. An intentionally disabled numerical regression is not a passing release result.
6. Retain conservative QP convexity classification. Expose PSD check outcome and fill/pivot limits; return Unsupported/Indeterminate rather than incorrectly accepting borderline curvature. Add a backward-error certificate before promoting near-singular cases.
7. Keep PDLP approximate termination distinct from a certificate. Recompute original-space residuals and objective gap, and return approximate/feasible or resource-limited when the independent gate fails.
8. For infeasible and unbounded LPs, verify Farkas multipliers and recession rays in original-space conventions. For MIP, distinguish “tree exhausted” from a replayed or independently audited proof; publish the global bound and complete search accounting.
9. Add metamorphic tests for row/column permutations, positive row scaling, objective scaling, equivalent bound encodings, duplicate sparse entries, and model serialization round trips.
10. Test ill-conditioned and degenerate cases with declared tolerance sweeps. Record numerical failure honestly; do not tune one fixture's tolerance to force an optimal status.

**Exit gate:** every accepted LP/QP optimal result passes the independent numerical verifier; MIP optimality also requires complete, audited search accounting. No false verified status appears on the adversarial suite, and failure and approximate statuses remain informative.

### Workstream C — Sparse scalability and controlled runtime

The current project stores sparse models, but the primal simplex and node-relaxation route still densify. Node search also copies model state at many points. Repairing sparse storage alone will not remove these cliffs.

1. Replace the dense tableau path for supported production-size LP/MILP relaxations with sparse revised-simplex or another sparse basis algorithm. Keep a small dense oracle for tiny test instances only.
2. Remove the unconditional dense node route beyond the explicitly documented small-model fast path. Select the node LP method from sparse structure and current basis; report the selected engine and fallback.
3. Make the root model immutable and shared. Represent child decisions as bound deltas and local cuts; use reusable per-worker workspaces and basis state rather than copying the complete matrix/model at each node.
4. Profile allocation, factorization fill, sparse basis updates, cuts, presolve and repeated solves. Add memory budgets and checked size arithmetic before every large allocation.
5. Carry one absolute deadline/cancellation token through input reading, canonicalization, presolve, scaling, root relaxation, cut generation, strong branching, factorization, callbacks, B&B workers, GPU transfers and postsolve. Add periodic checkpoints to long loops and preflight limits to indivisible operations.
6. On interruption, return the best valid incumbent and best valid bound with their timestamps; never lose a feasible incumbent or label incomplete search proven optimal.
7. Measure deadline overrun separately from solver work. Define a service envelope such as bounded overrun for non-preemptible kernels, document any remaining exceptions, and test tiny and already-expired deadlines across each engine.
8. Improve cut-loop scheduling and parallel load balance only against pinned cases. Keep parallel execution optional until repeated paired runs show stable benefit. A single speedup on one instance is not a general scalability claim.

**Exit gate:** sparse model size no longer forces dense-memory growth in the advertised path; node memory scales with workspace plus deltas, not full-model copies; time-limit behavior preserves valid proof state and has a measured overrun bound.

### Workstream D — Software architecture, API and supply-chain reliability

1. Split long modules by coherent responsibility. Do not make files pass by minifying, deleting comments, hiding logic in macros, or splitting into one-function fragments without meaningful interfaces.
2. Target **200–250 physical lines per maintained source file** so normal additions retain headroom under the hard 300-line ceiling. Check every tracked first-party C++, CUDA, Python, shell, test and build-script code file in CI. Maintain an explicit allowlist only for generated code and data. Include comments and blank lines in the count; do not game the check.
3. Refactor with characterization tests and stable private interfaces. Public headers should remain small and define contracts, not implementation. Each module should own one responsibility; avoid dependency cycles.
4. Remove library debug output and process-environment hooks from normal builds. Use structured, caller-controlled logging and per-solve diagnostics. Tests may inject log sinks explicitly.
5. Define API status, exceptions, cancellation, thread-safety, ownership, model lifetime, numerical options and compatibility policy. The present interface is C++; call it a C++ API unless a real, versioned extern-C ABI and C consumer tests are added.
6. Return named primal values, row activities/slacks, row duals, reduced costs, variable bounds, model names, objective, incumbent/bound/gap, verifier results, runtime, termination reason, and build/model identity. Wire infeasibility diagnosis into the supported API or explicitly mark it unavailable; do not leave it test-only while documentation advertises it.
7. Export a namespaced CMake target and package config with transitive dependencies. Verify install into a clean prefix with a separate consumer using find_package. Treat Python wheel packaging as a separate reproducible build.
8. Make default CI work from a clean checkout. Optional Python and comparison suites must either provision pinned dependencies or be explicitly separated and skip with an honest, documented reason. Do not rely on ignored local .venv paths.
9. Make a CUDA job fail if it did not enable CUDA and compile CUDA translation units. Keep CPU fallback coverage in a separately named job. Add deterministic compile coverage for optional ML if that build remains supported.
10. Add limits and fuzz/property tests for MPS, LP and NLOBJ parsers: input bytes, line lengths, dimensions, nonzeros, integer overflow, malformed exponents, duplicate data, invalid UTF-8/name bytes, nested markers, and allocation failure.
11. Run format, warnings-as-errors, sanitizers and thread-safety checks in a clean CI matrix. Treat benchmark integration as a distinct opt-in job with pinned external tools.
12. Review SPDX identifiers, dataset and third-party notices, pinned dependency/SBOM records and vulnerability status. Publish a versioned changelog, API compatibility policy and supported issue/security-reporting path. Tie source, binary and benchmark evidence to one reproducible release manifest.

**Exit gate:** clean checkout configures and builds; default CI runs deterministically; enabled CUDA means actual CUDA compilation; downstream C++ consumer links using the installed package; every maintained code file is at most 300 lines.

### Workstream E — Refinery-user workflow and result trust

1. Treat the solver core and a refinery planning application as separate deliverables. First validate one real, anonymized, engineer-owned case; do not add scheduling features before a process owner identifies the workflow.
2. Add stable variable, row and unit names in every input format and preserve them through presolve, canonicalization, postsolve and JSON output.
3. Include slacks, duals/shadow prices, reduced costs, active bounds, binding constraints, quality margins, warnings and certificate details. Explain sign and unit conventions for prices.
4. Add an infeasibility workflow: validated IIS/conflict output with named rows and bounds, irreducible status, and caveat where the IIS is heuristic or incomplete.
5. Define model versioning and scenario provenance: feed assays, yields, prices, limits, product specs, units, source files and user-approved overrides.
6. Provide a human-reviewable report and machine-readable output, plus input validation and reproducible reruns. Keep the current CLI as a solver interface, not a substitute for an operator workflow.
7. Run in shadow mode beside the incumbent planning process. Compare feasibility, objective/plan quality, constraint violations, analyst time and runtime. Require documented sign-off before production use.

**Exit gate:** refinery results are named, traceable, physically balanced, in-spec, independently checked and reviewed by the model owner. Otherwise describe examples as synthetic demonstrations.

### Workstream F — SIH, business case and benchmark discipline

1. Align the requirement coverage register with the official SIH problem statement. For each requirement state: implemented, independently verified, benchmarked, externally reviewed, or unproven. Do not collapse these into “MET.”
2. Present sovereignty and verifiability as current strengths. Do not claim solver-speed advantage, GPU acceleration, ML benefit, broad scale, or refinery readiness until matched evidence passes.
3. Keep GPU PDLP experimental while the measured RTX 2050 cases lose end-to-end to CPU. Either finish the GPU QP x-update and demonstrate end-to-end benefit or remove that acceleration claim and scope. A correct GPU residual product is not a GPU ADMM x-update.
4. Keep ML branching opt-in and unpromoted until the committed model is a valid standard artifact, training data are instance-disjoint and sufficiently broad, C++ inference matches a reference, and repeated trials improve verified solve outcomes (not just candidate scores or node count).
5. Use preregistered benchmark inputs and time limits, exact input and binary hashes, warmups, randomized/interleaved repeated runs, equal timing boundaries, versioned baselines, and complete status/objective/gap reporting. Include failures, unsupported inputs, timeouts and parent watchdogs.
6. Separate locally reproduced open-source solver comparisons from historical published tables. Do not imply contemporary commercial-vendor parity from withdrawn or non-overlapping benchmark results.
7. Report solve coverage alongside timing: verified optimal, optimal-within-gap, feasible, infeasible with certificate, timeout/resource limit, numerical failure, unsupported and invalid input.
8. For business validation, document target user, use case, incumbent tool, integration cost, support burden, licence/ownership benefit, solver operating cost and measurable pilot success criteria.

**Exit gate:** an evaluator can reproduce each displayed result from a clean checkout; business claims are supported by a shadow pilot or explicitly labeled hypotheses; SIH status rows cite artifacts and limitations.

### Workstream G — Repository size and documentation hygiene

The snapshot has **1,854 tracked files totaling 592,866,897 bytes (about 565 MiB)**. The data directory accounts for 588,027,318 bytes (99.18%); MPS files alone account for 556,642,371 bytes. Markdown is about 2.26 MB and is not the cause of clone size.

1. Keep small, redistributable smoke fixtures needed by default tests and the offline demo in Git. Determine the keep list by tracing CMake/CTest and GPU test paths to their input files. The large optional sets are concentrated in data/miplib, data/mittelmann, data/qp and data/netlib; preserve small data/cases fixtures.
2. Move large benchmark corpora to an optional local cache or versioned release artifact. Add a manifest of source URL, license/provenance, SHA-256, expected size and benchmark family; download only on explicit request and verify the hash. Never make the default build depend on network access.
3. Update runners to report “dataset unavailable” distinctly from solver failure. Add a small smoke suite for CI and a full opt-in suite for benchmark work. A missing required CI fixture must fail visibly; an optional full suite may skip only with an explicit dataset-unavailable status.
4. Add generated results and build directories to ignore rules only after confirming they are generated. Do not delete users' local build trees as repository cleanup.
5. Removing the current data from HEAD reduces checkout size, but old Git objects can still inflate clone history. History rewriting changes commit hashes and must be a separate coordinated migration with a backup and remote plan. Do not use filter-repo as a routine cleanup step.
6. Keep one current implementation plan, the current audit/findings register, the official problem statement, current capability/status docs, build/verification instructions, provenance and the research notes that support active algorithm decisions.
7. Remove stale duplicate roadmap snapshots, project feature inventories and agent orchestration logs only after replacing inbound links with the canonical plan or current audit. Keep changelog and unique historical evidence where they remain useful.
8. Add a Markdown-link checker and a claim-to-evidence check so removals and status edits do not leave dead references or unsupported capability claims.

**Exit gate:** core checkout contains only necessary code, small smoke inputs, provenance and current documentation; full benchmark data is fetchable with verified hashes; no active document points to removed audit snapshots.

## 4. Mandatory modularization inventory

At the reviewed snapshot, 48 tracked first-party code and build files exceed 300 physical lines. Every one is in scope; the list is the starting inventory, not an allowlist. Current physical line counts are included to make the work reviewable.

**Core solver and API**

- src/milp/milp_solver.cpp — 1,037
- src/api/api.cpp — 969
- src/lp/reference/revised_simplex.cpp — 824
- src/linalg/sparse_basis.cpp — 776
- src/lp/first_order/pdlp.cpp — 753
- src/milp/parallel_tree_search.cpp — 655
- src/presolve/presolve.cpp — 608
- src/minlp/minlp_solver.cpp — 571
- src/lp/dual/dual_simplex.cpp — 568
- src/lp/interior/ipm.cpp — 561
- src/io/lp_parser.cpp — 696
- src/io/mps.cpp — 540
- src/qp/model.cpp — 520
- src/milp/heuristics.cpp — 476
- src/nlp/sqp_solver.cpp — 469
- src/qp/admm_solver.cpp — 460
- src/milp/branch_selector.cpp — 364
- src/milp/node_lp.cpp — 347
- src/transform/sparse_canonicalize.cpp — 338
- src/io/nlobj_parser.cpp — 331
- src/milp/ml_branching/onnx_scorer.cpp — 403
- src/qp/kkt.cpp — 396
- gpu/src/pdhg_step.cpp — 501
- apps/cli_options.hpp — 323
- python/src/bindings.cpp — 535

**Benchmark, data and training tools**

- CMakeLists.txt — 459; split configuration, packaging and test registration into coherent included modules
- scripts/run_full_compare.py — 1,342
- scripts/import_qplib.py — 686
- scripts/run_gpu.py — 668
- scripts/plot_crossover.py — 558
- scripts/download_mittelmann_suite.py — 511
- scripts/generators/gen_literature_cases.py — 494
- scripts/run_compare.py — 452
- scripts/run_crossover_large.py — 380
- scripts/run_netlib.py — 367
- scripts/profile_gpu.py — 325
- scripts/dolan_more_profile.py — 313
- scripts/run_full_benchmark.py — 308
- scripts/ml/train_branching_gnn.py — 302

**Tests and acceptance harness**

- tests/e2e/test_tier1_m1_features.cpp — 699
- tests/e2e/test_tier2_m1_boundaries.cpp — 558
- gpu/tests/equivalence_test.cpp — 423
- tests/parallel_tree_search_test.cpp — 418
- tests/strong_branching_test.cpp — 381
- tests/qp_test.cpp — 363
- tests/milp_test.cpp — 360
- python/tests/test_bindings.py — 354
- gpu/tests/gpu_buffer_test.cpp — 325

Split tests by invariant or behavior, not just by line count. Share fixtures and assertion helpers in small modules. Re-run the same full inventory check after each refactor phase so newly added files cannot exceed the cap.

**Suggested module boundaries:** split `milp_solver.cpp` into root processing, node processing, proof accounting and result assembly; `api.cpp` into engine dispatch, postsolve, certificate gating and result mapping; `mps.cpp` into section parsing, bound semantics and model construction; `python/src/bindings.cpp` into model, options, result and callback bindings; `scripts/run_full_compare.py` into solver adapters, execution, evidence validation and reporting; and `CMakeLists.txt` into option, target, install and test modules. Keep hot data paths allocation-aware and profile before changing algorithms.

## 5. Acceptance sequence and release decision

Release in stages. The order is intentional: later performance and business evidence is not useful if the solver parsed a different mathematical problem.

1. **Correctness candidate:** integer MPS semantics fixed; parser conformance fixtures pass; original-space LP/QP witness checks and separate MIP search-proof accounting fail closed; all critical false-result cases are closed.
2. **Numerical candidate:** residual scaling, gap/status semantics, QP KKT signs, anti-cycling contract, and infeasibility/unbounded witnesses are verified across engines.
3. **Scalable candidate:** sparse node path and shared immutable model are in place; deadlines and memory budgets preserve incumbent/bound; large sparse cases have honest outcomes.
4. **Maintainable candidate:** all first-party source/test/tool files are modular and under 300 lines; package, CI, CUDA/optional-feature behavior and clean checkout are repeatable.
5. **Refinery candidate:** formulation reviewed by an engineer, named outputs/duals/IIS workflow works, and a shadow pilot meets agreed quality and runtime criteria.
6. **SIH/business candidate:** requirement matrix and benchmark claims are current and reproducible; no speed or industrial claim exceeds evidence.
7. **Repository candidate:** default checkout is small and offline-buildable; large optional corpora use verified manifests; docs and evidence links validate.
8. **Final closure audit:** independent reviewer checks the reconciled defect register and samples high-risk proofs, parser cases, benchmark records, refinery equations, source limits and clean-clone behavior. Publish the measured ≥90% defect closure and 100% critical/high closure only after this step.

Passing one stage does not imply passing later stages. Until all gates relevant to the advertised product pass, describe markov-cero as a research solver prototype with an experimental refinery demonstration.

## 6. Initial finding-to-work map

This map covers the currently observed open defects and material gaps. It is a seed for the deduplicated baseline register in Workstream A; historical resolved findings remain historical. Every item below has a fix and an exit gate in Sections 2–5. The final 90% calculation uses the reviewed register, not this preliminary count.

If all 36 remain separate and are classified as actionable defects after triage, the 90% threshold is **33 closed items**. The 100% critical/high gate still applies, so three open release-blocking items cannot be hidden by that percentage. Items classified as feature gaps need their own capability gates and are excluded from the defect percentage.

1. MPS marker/LO/LI bound overrides — P0 parser gate.
2. Unsupported or dialect-dependent MPS bounds, ranges and objective semantics — parser conformance gate.
3. Model-wide RHS tolerance in MILP node witnesses — local backward-error gate.
4. LP objective-gap and objective-consistency checks differing by engine — common certificate gate.
5. QP bound-side dual signs and complementarity — KKT gate.
6. QP PSD uncertainty and MIQP node-relaxation certification — QP/MIQP proof gate.
7. PDLP approximate stopping reported as verified optimal — original-space accuracy gate.
8. MILP nonzero-gap status and the large-bound-to-incumbent substitution — gap semantics gate.
9. MILP/MIQP search-tree, cut and node-bound proof accounting — audited ledger gate.
10. Infeasible/unbounded status without supported LP/QP witnesses — certificate-type gate.
11. EXPAND-named perturbation without the named method's guarantee — anti-cycling gate.
12. Disabled `regression_simplex_scale200_pricing` numerical case — default CI gate.
13. FCC product without a connected feed and yield balance — engineer-reviewed mass balance.
14. Unenforced refinery product-quality inputs and unit conventions — engineer-reviewed quality gate.
15. Pooling SLP fixed-point result without original nonlinear checks — local-result gate.
16. Unnamed primal vector and missing row dual/reduced-cost output — named-result API gate.
17. IIS/conflict functionality not exposed in the supported user path — diagnosis gate.
18. Dense simplex and node LP memory cliff — sparse-path memory gate.
19. Per-node full-model copies — immutable-model/delta memory gate.
20. Deadline overrun in preprocessing, root and other long work units — measured deadline gate.
21. Unbounded allocation/factorization fill in large cases — memory-budget gate.
22. Default CTest dependencies on ignored Python environments — clean-checkout CI gate.
23. CUDA job able to pass after CPU fallback — actual-CUDA compilation gate.
24. Missing consumer-ready CMake package export — clean-prefix consumer gate.
25. Library debug environment hooks and direct stderr output — structured-logging gate.
26. Forty-eight code/build files above the 300-line ceiling — modularization gate.
27. Parser adversarial and fuzz coverage gaps — bounded-input gate.
28. GPU QP ADMM x-update absent from the claimed device path — scope-or-implementation gate.
29. Invalid deployed ML artifact and insufficient generalization data — ML promotion gate.
30. Short-cap and timing-asymmetric benchmark comparisons — fair-evidence gate.
31. Large default tracked benchmark corpus — manifest/cache and small-checkout gate.
32. Stale capability and historical roadmap claims — documentation/evidence gate.
33. Missing release supply-chain and support policy — release-manifest gate.
34. No validated refinery shadow workflow or model-owner sign-off — domain acceptance gate.
35. In-tree cut and parallel scaling claims without broad paired evidence — repeatable performance gate.
36. Python binding packaging and ownership/zero-copy claims not fully specified — distribution contract gate.
