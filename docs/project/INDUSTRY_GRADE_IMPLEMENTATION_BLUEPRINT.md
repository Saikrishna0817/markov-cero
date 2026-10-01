# markov-cero: industry-grade solver implementation blueprint

**Planning baseline:** repository revision `fe86609` (2026-09-29).  
**Document status:** implementation plan, not a claim that every gate is closed.  
**Primary capability register:** [STATUS.md](STATUS.md).  
**Execution rule:** an implementer must not skip a prerequisite or turn an unverified result into a proven one to make a test pass.

This document incorporates the prior 35-section engineering blueprint and adds detailed execution plans for **MIQP, NLP, and MINLP**. It describes work to do. The repository already contains implementations; stages below generally mean *harden, measure, and qualify*, not *rewrite from zero*.

## How to use this document

1. Read Sections 1–9 to understand scope, contracts, and terminology.
2. Work through Section 27 task cards in dependency order. Do not begin a task whose prerequisites are open.
3. Before editing a numerical algorithm, copy its task's formulation, invariants, pseudocode, and failure rules into a short contract under `docs/contracts/`.
4. Implement one vertical slice per PR: contract → code → mathematical tests → end-to-end test → benchmark → documentation.
5. Attach raw commands, source SHA, binary SHA, model hashes, and outputs to the PR. A passing test count without those identifiers is not release evidence.
6. Use `docs/project/STATUS.md` to state what is supported today. This blueprint is a roadmap.

## 1. Executive Summary

markov-cero is an original C++ optimization solver prototype, not a runtime wrapper. It has a CSC model, MPS/LP parsers, primal/dual simplex, IPM, PDLP, sparse basis factorization, branch-and-cut, convex QP ADMM/KKT, MIQP relaxations, local SQP, restricted convex MINLP outer approximation, verifiers, a CLI, Python bindings, and an optional web adapter. The local existing build passed 91/91 CTest cases during the preceding audit. The production, benchmark, independent proof, and refinery gates remain open.

The shortest credible route is: freeze source and contracts; close correctness and resource risks; make LP sparse end to end; assure MILP and MIQP node bounds; qualify convex QP; explicitly scope local NLP and convex quadratic MINLP; then benchmark, package, and deploy. Do not add algorithm families simply to enlarge the feature list.

## 2. Current Repository → Target State

| Component | Current evidence | Target contract |
|---|---|---|
| Input/model | `include/markov_cero/model/model.hpp`, `src/io/` | One documented supported dialect and structural model invariant |
| LP | `src/lp/`, `src/api/engine_lp.cpp`; API can call `to_dense()` | Sparse canonical LP path; dense adapter only below an explicit size cap |
| Numerical algebra | `src/linalg/` sparse LU, eta updates, refinement | Verified solve residuals, bounded fill, reproducible failure statuses |
| MILP | `src/milp/` tree, cuts, heuristics | Every prune based on a valid bound/domain argument; honest gaps and limits |
| Convex QP | `src/qp/` ADMM/KKT and PSD screening | Original-space KKT/PSD contract and defined accuracy envelope |
| MIQP | `src/milp/node_qp.cpp`, `src/qp/supporting_bound.cpp`, MIP proof replay | Audited convex node lower bounds and independent bounded tree proof |
| NLP | `src/nlp/` SQP/L-BFGS, `src/api/engine_nonlinear.cpp` | Local stationary/KKT outcome with derivative and globalization tests; never global proof for nonconvex input |
| MINLP | `src/minlp/` convex-quadratic structural screen and OA master | Restricted convex quadratic MINLP with audited OA cuts/bounds; global assurance only after independent replay |
| Verification | `src/verify/`, `src/nlp/nlp_verifier.cpp`, `src/qp/verifier.cpp` | Original-space primal plus class-specific witness and explicit assurance level |
| Resources | `core::SolveContext`, instrumented charging | Honest cooperative library limits; hard service limits at an OS process boundary |
| Evidence | `evidence/`, `scripts/` | Committed frozen source, complete denominator, independent reproduction |
| Release | CMake/Python/CI/web adapter | Coherent license, supported matrix, SBOM, named maintainers, operational envelope |

## 3. Target Product Definition

### Supported v1 product

An installable C++ library and CLI supporting: LP, linear MILP, convex QP, and convex MIQP under a published size/numerical envelope. A thin Python binding is supported on a tested platform matrix. NLP is a **local optimization** feature, and MINLP is a **restricted convex quadratic research/advanced** feature until the later gates in Sections 27–31 close. The HTTP adapter is optional and executes the CLI in an isolated child process.

### Result semantics

Every result contains problem class, resolved engine, solver status, assurance level, primal vector if available, original objective, best valid bound where available, gap, residuals, iterations/nodes, stop reason, and model fingerprint. An incumbent is not a proof. `local_optimal` for NLP must be described in user documentation as a verified first-order local KKT candidate unless second-order sufficiency is checked. `optimal` from the current MINLP OA loop is solver-trusted and must not be presented as independently certified while `canonical_verified` is false (`src/api/engine_nonlinear.cpp`).

### Explicit exclusions

No general nonconvex QP/MIQP global optimization; no arbitrary callback MINLP global claims; no full-device QP claim; no ML speed claim; no refinery operational deployment claim without qualified domain approval.

## 4. Architectural Principles

1. Dependency direction: UI/HTTP → CLI/API → model/transforms/engines → numerical core. No reverse edges.
2. Keep `model::Model` and CSC. Do not introduce a second variable/constraint object hierarchy.
3. Keep concrete engine modules selected in `src/api/dispatch.cpp`; no factory/plugin framework.
4. Status, primal feasibility, and global assurance are separate fields.
5. Reversible transforms and original-unit verification are mandatory.
6. Keep dense conversion explicitly capped, measured, and confined to compatibility paths.
7. The least justified algorithm is the first one to defer. Feature count is not a quality measure.

## 5. Non-Negotiable Engineering Rules

- Write the formulation, assumptions, invariants, pseudocode, numerical failure modes, and tests before algorithm edits.
- Reject NaN/Inf in input, callbacks, iterates, factors, multipliers, and witnesses unless an infinity is an explicitly typed bound.
- Do not silently use an external optimizer in runtime solving. External solvers are oracles and baselines only.
- Do not convert a timeout, proof exhaustion, uncertain convexity result, or incomplete node relaxation into `Optimal`.
- Do not make a numerical test pass by increasing a tolerance without an error analysis and a reviewed acceptance bound.
- A speed improvement must preserve all accepted certificates and disclose regressions.
- Keep each PR limited to one mathematical contract or one measured bottleneck.

## 6. Technology Decisions

| Decision | Reason | Rejected option | Reconsider when |
|---|---|---|---|
| Keep C++20/CMake core | Existing code, package exports and tests | Rewrite in another language | Proven integration requirement |
| CSC primary matrix | Simplex pricing/basis assembly are column oriented | Generic multi-format matrix hierarchy | Profile shows row operations dominate |
| `double` solver arithmetic; long-double accumulation in checks | Existing ABI and memory characteristics | Arbitrary-precision core | A defined supported class demonstrably requires it |
| Existing sparse LU with bounded updates | Real tested implementation | Explicit basis inverse | Measured stability/performance failure |
| Distinct tolerances under one policy | Feasibility, integrality, pivot and KKT errors are different quantities | One global epsilon | Never without mathematical justification |
| Process boundary for hard hosted limits | Cooperative in-process checks cannot preempt every callback/allocation | Advertised hard in-process deadline | Complete allocator/cancellation proof exists |
| Keep exact engine actually used in result | Fallbacks can change algorithm | Report only requested engine | Never |
| Thin Python/web layers | Core must work offline | Duplicate solver logic in service | Never |

## 7. Target Directory Structure

Keep the existing `include/markov_cero/{model,io,linalg,lp,milp,qp,nlp,minlp,verify,api,core}`, corresponding `src/` modules, `apps/`, `python/`, `tests/`, `scripts/`, `evidence/`, and `web/`. Add only:

```text
docs/contracts/    reviewed mathematical contracts, one per changed algorithm
docs/decisions/    short ADRs for decisions that affect multiple modules
tests/corpus/      tiny exact fixtures and machine-readable expected properties
```

Do not move source files for cosmetic symmetry. Split a file only when its current responsibilities are genuinely separate or build/test isolation improves.

## 8. Mathematical Core Architecture

The core model is:

```text
LP/MILP: min or max cᵀx + c₀
          subject to row_lower ≤ Ax ≤ row_upper,
                     var_lower ≤ x ≤ var_upper,
                     x_j integral for declared j.
QP/MIQP: add ½xᵀPx, require P symmetric PSD for supported convex solving.
NLP:     min f(x), g(x) ≤ 0, h(x) = 0, lower ≤ x ≤ upper.
MINLP:   NLP form with integral variables; current global OA scope requires
         structurally represented convex quadratic objective/inequalities.
```

`Model::validate()` owns dimensions, CSC monotonic offsets, row-index range, finite coefficients, typed infinite bounds, bound ordering, type consistency, quadratic convention, and names. Classification must not mutate a model. Canonicalization records shifts, sign changes, row mappings and objective offsets. Hashes identify the exact validated model, not an informal filename.

## 9. Numerical Core Architecture

Create `docs/contracts/numerical-policy.md` before altering defaults. Document each current engine default from its option header, its scale, accepted range, and the verifier rule. Specify normalized row error, for example

```text
v_i = max(row_lower_i - (Ax)_i, (Ax)_i - row_upper_i, 0)
allowed_i = abs_feas + rel_feas * max(1, |row bound_i|, Σ_j |A_ij x_j|)
```

Use the finite active side only. Objective checking recomputes from the original model with wider accumulation. Integrality checks `|x_j-round(x_j)| ≤ int_tol` only for integer variables, plus binary bounds. LP canonical optimality requires primal feasibility, dual feasibility, and a bounded primal-dual gap; unboundedness requires a feasible anchor and improving recession ray; infeasibility requires a valid alternative witness. QP requires primal, stationarity, multiplier-side, and complementarity checks. A pivot tolerance is **not** a feasibility tolerance.

Factorization interface stays `factorize`, `solve`, `solve_transpose`, `replace_column`, `refactorize`, diagnostics and resource polling. After a basis solve, compute a backward residual; refine or refactorize if needed; fail explicitly if the bound remains violated. Do not call a pivot-ratio proxy a rigorous condition number.

## 10. LP Implementation Plan

For canonical `min cᵀx`, `Ax=b`, `x≥0`, maintain basis `B`, `x_B=B⁻¹b`, row multipliers `y=B⁻ᵀc_B`, and reduced costs `r_j=c_j-A_jᵀy`. A candidate optimum requires `x_B≥-ε_p` and `r_N≥-ε_d`. Phase I artificial objective determines feasibility; an improving direction `d` with `Ad=0`, `d≥0`, `cᵀd<0` establishes unboundedness after verification.

**Implementation order:** (a) keep current reference simplex and regression tests; (b) add a sparse canonical entry point using CSC columns directly; (c) make `engine_lp.cpp` select it without unconditional `to_dense()`; (d) retain a bounded dense compatibility path; (e) compare certificates and original solutions on the same corpus; (f) profile pricing, basis updates, and factor fill; (g) only then consider Devex/steepest-edge. Bland's [finite pivot rule](https://pubsonline.informs.org/doi/abs/10.1287/moor.2.2.103) remains the anti-cycling fallback, not a claim of good performance.

```text
factor B; solve x_B and y; check basis residuals
price admissible nonbasic columns
if none: verify primal/dual/gap, then return candidate optimum
solve d=B⁻¹A_entering
if no limiting positive d: build/verify ray, then return unbounded
select a stable feasible leaving row; update factorization
on update failure: roll back, refactorize or return numerical failure
```

## 11. MILP Implementation Plan

For minimization, every node lower bound `L` must be valid and the incumbent `U` must be a verified original-model feasible integer point. Prune only for proven empty domain/infeasibility or `L ≥ U - absolute_gap_tolerance` with declared numeric guard. `gap_satisfied` and `optimal` must remain distinct.

Keep the current node views, bounded frontier, branching, cuts and heuristics. First verify the size-dependent LP relaxation paths in `src/milp/node_lp.cpp`; an iteration-limited PDLP relaxation without a valid lower bound cannot prune. Then independently derive every cut family under canonical-to-original mapping. Finally test tree coverage, bound inheritance, incumbent update, and proof replay. [Reliability branching](https://www.sciencedirect.com/science/article/pii/S0167637704000501) is relevant to the existing strong/pseudocost path; it does not justify a performance claim without an ablation.

```text
pop node → propagate valid bounds → solve relaxation
if no verified lower bound: retain node or stop as inconclusive
if bound closes gap: prune
if solution integral: verify in original model and update incumbent
else add only valid cuts, re-solve if budget allows, then branch
enqueue children whose domains partition the parent's fractional domain
```

## 12. Convex QP Implementation Plan

Supported class: `min ½xᵀPx + qᵀx + c₀` subject to `l≤Ax≤u`, with `P=Pᵀ⪰0`. Keep the existing ADMM/KKT implementation. The PSD classifier returns three outcomes: PSD, nonconvex, indeterminate. The last one is unsupported; it is not safe to assume convexity. For each accepted optimum, independently recompute objective, primal residual, stationarity, active-side dual signs, and complementarity in original units. For singular PSD Hessians, test both a finite optimum and a flat optimal face. The [OSQP paper](https://web.stanford.edu/~boyd/papers/pdf/osqp.pdf) is a concrete reference for ADMM with a quasi-definite KKT system; copy no stopping rule without matching conventions.

## 13. MIQP Implementation Plan

### Exact supported class and current wiring

Convex MIQP adds integer variables to the convex QP above. `src/api/dispatch.cpp` selects `miqp`; `src/api/engine_milp.cpp` enters the MILP search; `src/milp/node_qp.cpp` solves continuous convex QP relaxations under node-specific bound overlays; `src/qp/supporting_bound.cpp` forms a numerical lower bound; `src/verify/mip_proof_relaxation.cpp` replays bounded cut-free trees. This is already functional on named tests, including `tests/qp_scenarios_1.cpp`. It is not a general nonconvex MIQP solver.

### Mathematical contract

At node domain `D`, the relaxation removes integrality but keeps all original linear rows and tightened variable bounds. If `x̂` is a QP point and `P⪰0`, convexity gives `f(z)≥f(x̂)+∇f(x̂)ᵀ(z-x̂)`. The implementation also uses verified row multipliers and minimizes the resulting affine Lagrangian bound over the node box. A finite returned `L(D)` must satisfy `L(D)≤min{f(z): z∈D and original rows hold}` up to a **conservative downward numerical guard**. If an endpoint needed by the affine box infimum is infinite, return *no finite bound*, not the QP primal objective. The code currently fails closed this way in `src/qp/supporting_bound.cpp`.

### Required invariant and work sequence

1. Document the quadratic MPS convention once: off-diagonal terms, the `½` factor, objective sense, and offset. Test `xᵀPx/2` against hand arithmetic.
2. Check PSD of the **node's original quadratic objective** before any global bound. Indeterminate PSD → unsupported/inconclusive.
3. Apply node bound overlays to the QP *and* to the supporting-bound minimization. `tests/qp_scenarios_1.cpp` contains a starting regression.
4. Verify the QP KKT witness, then independently recompute the lower-bound expression with `long double` accumulation and directed downward guard. Never use `sol.objective_value` as a lower bound by itself.
5. Verify each incumbent in the original MIQP, including integrality, linear rows and quadratic objective.
6. On branch, children are `x_j≤floor(x̂_j)` and `x_j≥ceil(x̂_j)` intersected with parent bounds; reject non-partitioning splits.
7. Replay bounded cut-free proof trees using the node bound overlay and QP lower-bound checker. Cut-enabled search may retain an incumbent, but no global proof claim until cut obligations are independently validated.
8. Report proof exhaustion, node cap, QP iteration cap and numerical failure separately.

### MIQP pseudocode

```text
validate convex quadratic source and integer types
queue root with a valid inherited lower bound
while queue nonempty and resource budget remains:
    node = pop()
    q = continuous QP with node bound overlay
    qp_result = solve(q)
    if verified QP infeasibility: prune node
    else if verified KKT point and finite supporting lower bound:
        L = conservative_supporting_bound(qp_result, node box)
        if L closes gap to verified incumbent: prune
        else if QP point integer-feasible: verify original MIQP; update incumbent
        else branch on a fractional integer variable; enqueue exhaustive children
    else: stop/retain node as inconclusive; do not prune
return incumbent, global bound, gap, status and assurance tier
```

### Required MIQP cases

- Binary `y∈{0,1}`, `min(y-0.3)^2`: relaxation optimum `0`, integer optimum `0.09`. Check bounds at root and children.
- `min(x-1)^2+0.2y`, `0≤x≤2`, `y∈{0,1}`, `x≤y`: optimum `(1,1)`, objective `0.2`.
- Node with unbounded box but linearly bounded feasible region: a missing finite box support must be inconclusive, never fabricated.
- Nearly PSD and indefinite Hessians: indeterminate/nonconvex must not produce an optimum.
- Maximization and nonzero offset: output objective and bound signs must reverse correctly.
- Deliberately corrupt one node's bound, branch split, incumbent or QP multiplier: proof replay must reject.

### Exit gate

MIQP is supported only after all named cases, randomized small-domain enumeration, original-model verification, bounded proof replay, and held-out convex QP/MIP comparisons pass with no false global claim. Performance is a separate gate.

## 14. NLP Implementation Plan

### Exact scope and current wiring

`include/markov_cero/nlp/nlp_model.hpp` accepts objective/gradient, inequality values/Jacobian, equality values/Jacobian and bounds as callbacks; the NLOBJ/NLCON bridge also constructs polynomial callbacks. `src/nlp/sqp_solver.cpp` solves convex QP subproblems with L-BFGS curvature, a merit line search, a local step cap and limited Hessian resets. `src/nlp/nlp_verifier.cpp` checks first-order KKT conditions. `src/api/engine_nonlinear.cpp` maps an accepted SQP KKT point to `local_optimal`. The supported guarantee is **local first-order stationarity and feasibility**, not global optimality for nonconvex NLP and not necessarily a strict local minimum.

### Mathematical contract

`min f(x)` subject to `g_i(x)≤0`, `h_k(x)=0`, and `l≤x≤u`. Assume continuously differentiable callbacks on the evaluated domain and finite returned values. At iteration `k`, solve a convex QP in step `d`:

```text
min_d ½dᵀB_kd + ∇f(x_k)ᵀd
g(x_k)+J_g(x_k)d ≤ 0
h(x_k)+J_h(x_k)d = 0
l-x_k ≤ d ≤ u-x_k
```

`B_k` is the positive-definite L-BFGS approximation. A merit function combines objective and violation. At an accepted candidate, compute primal residual, nonnegative multipliers for `g≤0`, equality multipliers, bound-normal stationarity, and complementarity. First-order KKT is **necessary under constraint qualification**; it does not establish a local minimum at a saddle. A second-order sufficiency check would be a separate task and status tier. The [Nocedal–Wright SQP chapter](https://link.springer.com/book/10.1007/978-0-387-40065-5) is the algorithmic reference.

### Work sequence

1. Validate callback dimensions, bounds, finite outputs, and exception behavior at **every evaluation**; include objective/gradient/Jacobian calls within solve-wide deadline checks where feasible. A callback can block indefinitely: hosted hard limits require subprocess isolation.
2. Add a developer-only derivative checker: centered finite differences in the interior, one-sided near bounds, step based on machine precision and scale. It diagnoses user callbacks; it does not silently replace analytic derivatives in production.
3. Compare `NLOBJ` polynomial objective/gradient and `NLCON` Jacobian against analytic hand values, including cross terms, maximization and offsets.
4. Audit SQP QP subproblem construction, multiplier sign mapping, Hessian reset, line-search descent and step cap. Do not state “strong Wolfe always holds”: current code permits an Armijo fallback for nonsmooth merit.
5. Add an explicit **restoration/elastic** subproblem for inconsistent linearized constraints only after the present failure modes are measured. Until then, return an honest numerical/inconclusive status, not infeasible.
6. Return the best **verified feasible** point on iteration/time limits, separate from a KKT point. Define whether `x0` is projected to bounds and report that projection.
7. Change the public description/status schema so a checked first-order point is `local_stationary` (or retain the enum for compatibility while explicitly labeling its assurance `local_kkt_only`). Reserve `local_minimum_verified` for a future second-order test.
8. Test several starts for nonconvex examples and never infer globality from agreement of starts.

### NLP pseudocode

```text
validate model and initial point; project initial point to bounds
for each major iteration:
    evaluate f, grad, g, Jg, h, Jh; reject nonfinite/dimension mismatch
    build convex QP step model from L-BFGS Hessian approximation
    solve QP under shared deadline; verify QP step
    if QP fails: reset Hessian within budget, else report inconclusive
    compute merit descent direction; line-search with bounded work
    if accepted: update x and L-BFGS curvature pairs
    recompute original nonlinear residuals and KKT conditions
    if first-order contract passes: return local KKT candidate
return limit/failure plus any independently feasible point
```

### Required NLP cases

- Unconstrained convex quadratic with known minimizer and gradient.
- Rosenbrock from at least two starts; report local outcome and iterations.
- Equality-constrained quadratic with analytic multiplier.
- Active upper/lower bounds and changing active sets; test multiplier signs.
- Deliberately wrong gradient/Jacobian callbacks; derivative diagnostic detects mismatch.
- Saddle `f(x,y)=x²-y²` at `(0,0)`: zero gradient **must not** be called a verified local minimum.
- Infeasible constraints `x≤0`, `x≥1`: do not report `Optimal`, `LocalStationary`, or globally proved infeasible merely because SQP fails.
- Callback returns NaN, wrong-sized vector, throws, or stalls: bounded error/stop behavior.

### Exit gate

NLP can be advertised as a **local SQP solver** after callback validation, derivative diagnostics, residual/KKT verification, status semantics, pathological tests and documented size/time limits. Global nonconvex optimization remains out of scope.

## 15. MINLP Implementation Plan

### Exact scope and current wiring

`src/minlp/minlp_solver.cpp` screens the structurally represented quadratic objective and NLCON Hessians, currently limits screening to 512 variables, rejects arbitrary callback companions, and rejects nonlinear equality callbacks that the OA master cannot represent. `src/minlp/minlp_solver_iterate_outer_approximation.cpp` alternates SQP subproblems with MILP masters. `src/api/engine_nonlinear.cpp` verifies incumbents but sets `canonical_verified=false`; there is **no independently replayed global OA proof**. Treat current global bounds as solver-trusted.

### Mathematical contract

Minimize differentiable convex `f(x)` subject to convex `g_i(x)≤0`, affine equalities/linear rows represented exactly, variable bounds, and selected integer variables. For any expansion point `p`, convexity gives

```text
f(x) ≥ f(p)+∇f(p)ᵀ(x-p)
g_i(x) ≥ g_i(p)+∇g_i(p)ᵀ(x-p)
```

Thus `eta ≥ f(p)+∇f(p)ᵀ(x-p)` and `g_i(p)+∇g_i(p)ᵀ(x-p)≤0` are valid **relaxation** rows; they may admit infeasible original points. The OA MILP master's valid lower bound and a separately verified original MINLP incumbent give a gap. A lower bound from an incomplete MILP master is usable only if that master's own bound is valid. A master unbounded ray does **not** prove the nonlinear original unbounded. The classic [Duran–Grossmann OA paper](https://doi.org/10.1007/BF02592064) and [Bonami et al. convex MINLP framework](https://www.sciencedirect.com/science/article/pii/S1572528607000448) define the literature basis; copy their assumptions, especially convexity and subproblem correctness, into the contract.

### Work sequence

1. Freeze exact supported class: structurally represented quadratic polynomial functions, PSD objective Hessian after objective-sense normalization, PSD Hessian for each `g≤0`; linear equalities stay exact in the master. Reject callback-only nonlinear MINLP unless a separately checkable convexity certificate is later designed. Do not widen scope by name alone.
2. Derive NLOBJ, QUADOBJ and NLCON Hessian assembly for diagonal and cross terms. Unit-test the `½P` convention and maximize sign before any OA test.
3. Make convexity screening fail closed on indeterminate PSD, fill/deadline exhaustion and malformed terms. Preserve the current 512-variable cap until data support raising it.
4. Ensure each OA cut stores expansion point, function value, gradient, objective sense, source row and numeric weakening. Independently replay the cut from the source polynomial rather than trusting stored coefficients.
5. Verify fixed-integer NLP subproblem feasibility in original model before accepting its objective as an upper bound. A local SQP KKT point is enough for a feasible incumbent if primal/integrality checks pass; it is **not** enough by itself for a global lower bound.
6. Verify every master lower bound, including early stops, through the MILP assurance layer. If that layer returns no certified bound, keep the incumbent and report `Feasible`/`Inconclusive`, not `Optimal`.
7. Handle initial unbounded master with a valid finite epigraph lower bound only when derivable from model bounds/convexity; otherwise report inconclusive. Do not insert an arbitrary big-M.
8. Support affine equalities exactly; keep nonlinear equality rejection until a mathematically valid representation is specified. A nonlinear equality is generally nonconvex as a feasible set even when its function is convex.
9. Add an independent OA proof replay format only after cut and master-bound replay are complete. Before that, API global assurance stays false even if internal gap closes.
10. Measure master growth, number of SQP failures, cuts, MILP nodes and total time; cap each with an explicit stop.

### MINLP pseudocode

```text
validate source structure and convexity; reject unsupported equality/callback cases
solve continuous NLP relaxation; keep only verified feasible incumbent if integral
repeat under shared resource budget:
    choose integer assignment from a verified OA master candidate
    solve fixed-integer NLP; independently check original feasibility/objective
    if feasible and better: update incumbent U
    at evaluated point, derive valid objective/constraint tangent rows
    add rows to OA master with provenance and numeric guard
    solve MILP master; accept only its verified lower bound L
    if L > U beyond error allowance: numerical failure
    if U finite and certified gap closes:
        return solver-trusted gap result, or globally verified result only if
        full OA proof replay is accepted
    if master unbounded/inconclusive: stop without global claim
return feasible incumbent and valid bound, or explicit limit/failure
```

### Required MINLP cases

- `x∈[0,2]`, `y∈{0,1}`, `min (x-1)²+0.2y`, `x²≤y`: optimum `(1,1)`, objective `0.2`; verify convex tangents at `x=0`, `0.5`, `1` never exclude feasible original points.
- Same model with affine equality, represented exactly; no nonlinear-equality callback accepted.
- Concave objective or `-x²≤0`: return nonconvex/unsupported, not global optimum.
- Maximize a concave quadratic with offset: normalized internal bound and user-facing sign/offset agree.
- OA master unbounded while nonlinear feasible region is bounded: return inconclusive, not original unbounded.
- SQP failure with a valid expansion point: cuts may remain valid, but no feasible incumbent from failed SQP without original checks.
- Master time/node limit: retain a verified incumbent and only a valid master bound.
- Corrupt a stored tangent gradient/value or source row: independent cut replay must reject.

### Exit gate

Restricted convex quadratic MINLP is *experimental* until Hessian assembly, tangent validity, master bounds and incumbent checks pass differential tests. It becomes **globally certified** only after independent replay accepts the complete OA/master evidence. Do not advertise arbitrary callback or nonconvex MINLP support.

## 16. Presolve & Scaling Plan

Retain `src/presolve/` records and `src/scale/ruiz_scaling.cpp`. For each current reduction, document precondition, equivalence proof, exact primal/dual reconstruction, tolerance boundary, a presolve-on/off differential test, and original-model witness. Prioritize existing empty/singleton/fixed/forcing/duplicate/dominated rules. Do not add aggressive doubleton aggregation or coefficient dropping before the current rules pass adversarial scale tests. Compare residuals in original units after unscaling. Andersen and Andersen's [presolve study](https://doi.org/10.1007/BF01586000) is a research starting point; verify its exact assumptions before matching an implementation.

## 17. Model / Parser Architecture

Keep `Model`, MPS and LP parsers. Publish supported sections and deliberate exclusions (including multiple rim vectors). Enforce byte, line, token, row, column, nonzero and name limits before allocations. Reject malformed numeric tokens and trailing data. Parser errors include format, line and reason. A parser does not call a solver. JSON remains output/reporting unless a concrete input schema requirement arises. Add parser corpus tests for `QUADOBJ`/`NLOBJ` cross-term conventions because MIQP/MINLP validity depends on them.

## 18. Solution Verification Architecture

Use two boundaries. **Original-model checker:** dimensions, row/variable bounds, integrality, objective and finite values. **Class-specific checker:** LP dual/ray/Farkas; convex QP KKT/PSD; MILP/MIQP branch tree and bounds; NLP first-order KKT; convex MINLP OA tangents/master bounds. A verifier recomputes activities and objective from input and must not trust solver telemetry. Publish `original_primal_checked`, `optimality_witness_checked`, `tree_replayed`, `local_kkt_checked`, `oa_replayed`, or `unverified` assurance. Current enum/API names may be adapted with schema versioning; do not break existing callers silently.

## 19. Testing Architecture

Default CTest remains fast and deterministic. Add tiny exact fixtures, property tests (permutation, row scaling, objective offset, presolve on/off, warm/cold), witness mutation, pathological numerics, differential oracle tests, fuzz/resource cases, and end-to-end `parse→solve→verify→JSON` tests. The default suite must not depend on ignored local Python environments or installed commercial solvers. Optional oracle campaigns run in separate CI/jobs. Keep every counterexample as a minimized regression case; do not assert only that a function returned a plausible status.

## 20. Benchmark Architecture

Freeze manifests before tuning. Use the official [Netlib LP collection](https://www.netlib.org/lp/), [MIPLIB benchmark set](https://miplib.zib.de/tag_benchmark.html), and supported convex subset of [QPLIB](https://qplib.zib.de/). Add explicit MIQP instances, analytic/local NLP fixtures, and structurally convex MINLP instances as **separate strata**. Never compare local NLP stationarity against a global objective as if both were global proofs. Record all attempted instances, including parse failures, unsupported results, timeouts, wrong witnesses and crashes.

Each run row must include model SHA, source/binary SHA, CPU/GPU/RAM/OS, compiler, options, threads, cap, comparator version, status, assurance, objective, bound, gap, residuals, wall/CPU time, peak RSS, iterations, nodes, cuts and raw log paths. Separate cold process and repeated-session studies. Use at least five repeats for a promoted speed claim, report paired distributions and a confidence interval, and reproduce on another host.

## 21. Performance Engineering Plan

Instrument parse, canonicalization, presolve, scale, dense conversion, basis factorization, pricing, node QP/LP, cut separation, SQP callback evaluation, OA master build/solve, verification and proof replay. Profile four frozen traces first: sparse LP, degenerate LP, hard MILP/MIQP, and convex QP; add local NLP and OA MINLP traces once correctness gates pass. Record factor nonzeros/refactorizations, callback count/time, master rows and peak RSS. Optimization protocol: measure → profile → state hypothesis → change one bottleneck → rerun correctness → compare raw data → document tradeoff.

## 22. API / CLI Plan

Preserve `api::SolveOptions` and `SolveResult`; evolve with a versioned result schema. Public concepts: `validate(model)`, `solve(model, options)`, `verify(model, candidate/witness)`; `presolve(model)` is advanced because it must return a reconstruction record. CLI accepts supported MPS/LP and reports **resolved** engine/backend, actual status, stop reason and assurance. Python remains a thin C++ binding. The current SQP and OA option values are not all exposed through top-level `SolveOptions`; expose only reviewed, meaningful knobs after each domain's contract is stable. No generic configuration framework.

## 23. Deployment Architecture and Security

Local CMake package and CLI are first. Then a tested Python wheel matrix. The optional `web/backend/server.py` adapter invokes the CLI in a child process; production must add OS-level CPU, address-space/RSS, output, file and wall limits and per-user rate controls. Library deadlines and memory charging remain useful cooperative controls but are not a hard RSS or callback wall guarantee. Keep `/health`, request IDs, bounded logs, support contact and rollback instructions. No database, queue, Kubernetes or distributed scheduler is required for v1. Resolve `pyproject.toml`'s “Proprietary” label versus the checked-in Apache 2.0 `LICENSE` before publishing a release.

## 24. Research Paper / Algorithm Map

| Component | Primary reference | Extract and compare with code |
|---|---|---|
| Simplex anti-cycling | [Bland 1977](https://pubsonline.informs.org/doi/abs/10.1287/moor.2.2.103) | Exact entering/leaving tie rules and finiteness assumptions |
| Reliability branching | [Achterberg, Koch and Martin 2005](https://www.sciencedirect.com/science/article/pii/S0167637704000501) | Probe and pseudocost update rule |
| Convex QP ADMM | [Stellato et al.](https://web.stanford.edu/~boyd/papers/pdf/osqp.pdf) | KKT matrix, residuals, stopping and infeasibility logic |
| Sparse basis updates | [Forrest and Tomlin 1972](https://doi.org/10.1007/BF01584548) | Actual update versus current eta/refactorization path; do not mislabel |
| LP presolve | [Andersen and Andersen 1995](https://doi.org/10.1007/BF01586000) | Preconditions and dual reconstruction |
| SQP and globalization | [Nocedal and Wright, Numerical Optimization](https://link.springer.com/book/10.1007/978-0-387-40065-5) | QP step, merit descent, restoration, local KKT interpretation |
| Convex MINLP OA | [Duran and Grossmann](https://doi.org/10.1007/BF02592064); [Bonami et al.](https://www.sciencedirect.com/science/article/pii/S1572528607000448) | Convexity, tangent validity, fixed-integer NLP and master lower bound |
| GMI/MIR | Existing `docs/research/algorithms/` notes, then verified primary papers | Derive validity under variable transformations; some local bibliography metadata is unverified |

For every changed algorithm record: source, exact problem, assumptions, equations, pseudocode, deviations, tests, benchmark and known limits. Do not cite a paper simply because the method name appears in a comment.

## 25. Architectural Decision Records

Create short ADRs only as changes land: ADR-001 CSC primary storage; 002 named numerical tolerances; 003 sparse basis LU/update; 004 bounded dense adapter; 005 status versus assurance; 006 node views and verified bounds; 007 frozen benchmark method; 008 NLP local-status semantics; 009 convex MINLP OA scope. Each ADR states decision, reason, rejected alternative, consequence and an evidence trigger for reconsideration. Keep each to one page.

## 26. Phase-by-Phase Implementation Plan

| Phase | Objective / modules | Research and tests | Benchmark / acceptance | Risk |
|---|---|---|---|---|
| 0 Freeze | License, committed source, clean CI, evidence manifests | Existing CTest and consumer build | Same SHA across tests/docs/binary | Historical evidence misattributed |
| 1 Contracts | Model, statuses, numerical policy, verifier | Tiny exact LP/QP/MIP and callback cases | All constructed false witnesses rejected | Wrong sign/tolerance convention |
| 2 Resources | SolveContext, parsers, factors, callbacks, service | Cancellation, allocation, worker kill | Measured cooperative and hard service envelope | Claiming hard in-process caps |
| 3 Sparse LP | Canonical model, simplex, dual API | Netlib, degeneracy, infeasible/unbounded | No unbounded dense allocation; same certificates | Basis index/reconstruction bugs |
| 4 MILP | Node relaxations, cuts, proof | Enumeration and proof mutation | Valid pruning and complete denominator | False bound/cut |
| 5 Convex QP | PSD, ADMM/KKT | Analytic/QPLIB cases | Original-space KKT accepted | Indeterminate curvature misread |
| 6 MIQP | Node QP, supporting bound, tree replay | Small enumeration and bound attacks | No false global MIQP claim | Numeric lower-bound overestimate |
| 7 NLP | SQP, callbacks, KKT/status | Rosenbrock, active bounds, saddle, wrong derivatives | Local status only; callback envelope | Mistaking KKT for minimum |
| 8 MINLP | Convexity, tangent cuts, OA master, replay | Analytic convex examples and cut attacks | Solver-trusted versus replayed assurance explicit | Invalid cut/master bound |
| 9 Performance | Stage traces and one bottleneck per PR | Full regression | Held-out paired result with all failures | Overfitting |
| 10 Interfaces | API/CLI/Python/schema | Round-trip and installed consumer | Stable versioned output | Compatibility break |
| 11 Deploy/docs | Optional isolated service, contracts, guides | Abuse/load/docs-link tests | Supported matrix and rollback | Resource escape |
| 12 Release | Hosted matrix, SBOM, independent review | Sanitizer/CUDA/second host | Signed and reproducible artifacts | Unowned maintenance |

Each phase exit requires its task cards' acceptance conditions, not just successful compilation.

## 27. Dependency Graph and Parallel Workstreams

```text
Frozen committed baseline
  → model + numerical + assurance contracts
  → resource envelope ───────────────────────────────┐
  → sparse LP → trusted node LP → MILP proof ────────┤
                 convex QP → trusted node QP → MIQP ──┤
                 convex QP → SQP local NLP ───────────┤
                               ↓                      │
               structural convexity + OA MINLP ──────┤
  → held-out differential/performance campaign ←─────┘
  → package/service qualification → release
```

Corpus/oracle tooling, docs and packaging can run alongside core work. Only one integration owner should change `Model`, `SolveOptions/SolveResult`, tolerance policy, or witness schemas at a time. Mathematical work on LP bounds, MIQP bounds and OA bounds is tightly coupled; do not parallelize those contracts merely to increase throughput.

## 28. Detailed Task Backlog

The task cards below are deliberately repetitive: each can be handed to an implementer without requiring them to invent architecture or acceptance rules. Paths name existing files unless explicitly marked *new*. Use the smallest reviewable PR that completes a whole card; split a card only at a mathematical boundary and keep its final acceptance gate open until all slices land.

### TASK BASE-01 — Freeze source, claims and release metadata

**OBJECTIVE:** Establish a committed, reproducible baseline for every later change.

**WHY IT EXISTS:** Historical evidence includes pre-commit worktrees; current results cannot be attributed to a source revision by filename alone. `pyproject.toml` says `Proprietary` while `LICENSE` is Apache 2.0.

**PREREQUISITES:** None. Obtain the license owner's decision; do not guess it.

**FILES / MODULES AFFECTED:** `pyproject.toml`, `LICENSE`, `README.md`, `docs/project/STATUS.md`, `.github/workflows/ci.yml`, `evidence/` manifest (new dated record).

**MATHEMATICAL BASIS:** The supported-class definitions in Sections 8 and 3; no algorithm change.

**RESEARCH REFERENCES:** Existing source and status register; no paper required.

**IMPLEMENTATION STEPS:** (1) Record `git status`, revision and submodule/dependency state. (2) Resolve license label. (3) Run a clean CPU CMake configure/build outside prior build dirs. (4) Run default CTest, Python binding tests after installing the built wheel, CLI class smoke, and installed consumer. (5) Record compiler, flags, binary hashes, commands, exit codes and raw logs. (6) Update claims only to the measured state.

**ALGORITHM:** Freeze and reproduce; no numerical algorithm.

**PSEUDOCODE:** `commit → clean build → hash binary → test → attach raw outputs → update status`.

**INTERFACES:** No public API change.

**EDGE CASES:** An optional CUDA/ML test must not silently pass through CPU fallback and be reported as a device/model validation.

**NUMERICAL CONSIDERATIONS:** Test tolerances/configuration recorded verbatim; do not retune them here.

**TESTS REQUIRED:** Default CTest, package consumer, bindings; verify a solved LP/MILP/QP returns expected result schema.

**BENCHMARK REQUIRED:** Baseline smoke only; store raw times but make no speed claim.

**DOCUMENTATION REQUIRED:** Release state, license, support matrix draft, exact commands.

**ACCEPTANCE CRITERIA:** A clean committed source SHA and binary SHA reproduce the tests on one clean environment; license metadata is coherent; historical evidence is explicitly labelled historical.

**EXPECTED OUTPUT:** Baseline manifest and honest capability page.

**DEPENDENCIES:** Blocks every later benchmark and release claim.

**ESTIMATED COMPLEXITY:** Medium engineering, low mathematics.

**RISKS:** Hidden local dependency or stale generated artifact.

**DO NOT DO:** Do not rewrite algorithms, change numerical defaults, or call an existing build directory a clean build.

### TASK NUM-01 — Version the numerical and status contract

**OBJECTIVE:** Define exact pass/fail semantics for all model classes.

**WHY IT EXISTS:** LP, MILP, QP, SQP and OA currently use distinct tolerances. This is legitimate only if each has a documented meaning and verification boundary.

**PREREQUISITES:** BASE-01.

**FILES / MODULES AFFECTED:** `include/markov_cero/api/solve.hpp`, engine option headers, `src/verify/`, `src/qp/verifier.cpp`, `src/nlp/nlp_verifier.cpp`; new `docs/contracts/numerical-policy.md`.

**MATHEMATICAL BASIS:** Original-space row/bound residuals, LP duality, QP/NLP KKT, integrality, relative gap.

**RESEARCH REFERENCES:** Nocedal–Wright for nonlinear KKT/SQP; existing LP/QP contract docs; verify any formula directly before use.

**IMPLEMENTATION STEPS:** (1) Inventory every tolerance and status use with `rg`. (2) For each, write quantity, units, formula, default, valid range, producer and checker. (3) Separate solver convergence from verifier acceptance. (4) Add typed assurance labels to result schema without silently changing old fields. (5) Add boundary tests around each threshold. (6) Document that `local_kkt` and `solver_trusted_oa` are not global certificates.

**ALGORITHM:** Compute residuals from original data with wider accumulation; compare each against the declared absolute/relative threshold for that quantity.

**PSEUDOCODE:** `validate finite → recompute original activities/objective → calculate scaled residuals → check class witness → set assurance tier → derive public status`.

**INTERFACES:** Add a schema version/assurance field if needed; preserve existing caller compatibility or provide a migration note.

**EDGE CASES:** Huge unrelated row RHS, zero objective, maximize sign, equality rows, free bounds, NaN dual, near-integer values, negative zero.

**NUMERICAL CONSIDERATIONS:** Avoid a global norm that lets one huge row hide a small-row violation; never use pivot tolerance as primal tolerance.

**TESTS REQUIRED:** Mutate accepted witnesses on both sides of the tolerance; assert rejected/accepted as specified. Keep existing `tests/readiness_correctness_test.cpp` green.

**BENCHMARK REQUIRED:** Measure verifier overhead on representative sparse models; no speed target yet.

**DOCUMENTATION REQUIRED:** Numerical policy, status/assurance table, migration note.

**ACCEPTANCE CRITERIA:** Every accepted global status names a passing witness checker; every tolerance has one documented meaning; no constructed false witness is accepted.

**EXPECTED OUTPUT:** Versioned mathematical status contract.

**DEPENDENCIES:** Needed by LP, MILP, QP, MIQP, NLP and MINLP task cards.

**ESTIMATED COMPLEXITY:** High conceptual effort, medium code effort.

**RISKS:** Objective-sense or multiplier-sign mistakes.

**DO NOT DO:** Do not set all tolerances to the same number or loosen checks to preserve old test statuses.

### TASK RES-01 — Close deadline and memory envelope gaps

**OBJECTIVE:** Make resource stops predictable and accurately described.

**WHY IT EXISTS:** IR-20/IR-21 remain open: callbacks, factorization, proof and device allocations are not uniformly preemptible/charged.

**PREREQUISITES:** BASE-01, NUM-01.

**FILES / MODULES AFFECTED:** `include/markov_cero/core/solve_context.hpp`, `src/linalg/`, `src/qp/`, `src/milp/`, `src/nlp/`, `src/minlp/`, `web/backend/server.py`.

**MATHEMATICAL BASIS:** A stopped solve may expose only previously verified incumbent/bound values; incomplete work never proves a status.

**RESEARCH REFERENCES:** None required for the resource accounting contract; OS-level limit documentation must be checked for the deployment platform when implemented.

**IMPLEMENTATION STEPS:** (1) List every allocation and indivisible operation. (2) Add shared-context poll/charge where bounded work is possible. (3) Measure maximum polling interval on adversarial models. (4) Preserve valid incumbent/bound on all stop paths. (5) For hosted service, spawn each solve with OS CPU/memory/file/output/wall limits and kill-on-timeout. (6) State that native library limits are cooperative unless a genuinely complete allocator contract is proved.

**ALGORITHM:** `before costly stage → poll; before owned allocation → charge; after stop → return resource status and only verified prior values`.

**PSEUDOCODE:** `if cancelled/deadline: stop; if reserve(bytes) fails: stop; perform bounded unit; release/account; repeat`.

**INTERFACES:** Existing `total_time_limit_seconds`, `memory_limit_bytes`, `stop_reason`; add only measured peak/poll diagnostics that are stable.

**EDGE CASES:** Deadline in parser, sparse LU, QP KKT, SQP callback, OA master, proof replay, parallel worker and JSON serialization.

**NUMERICAL CONSIDERATIONS:** Interrupted factorization is invalid; do not reuse partial factors. Bound validity must survive interruption.

**TESTS REQUIRED:** Worker kill, callback stall in child, allocation failure injection, factor fill overflow, tiny deadline, repeated solve after failure.

**BENCHMARK REQUIRED:** Peak RSS and wall overrun distributions on small/large pathological inputs.

**DOCUMENTATION REQUIRED:** Library cooperative contract and hosted hard-limit contract as separate pages.

**ACCEPTANCE CRITERIA:** No false optimal/infeasible/unbounded status after stop; published measured envelope; service child stays inside declared OS caps in tests.

**EXPECTED OUTPUT:** IR-20/IR-21 closure evidence or explicit remaining limitation.

**DEPENDENCIES:** Required before production service/release.

**ESTIMATED COMPLEXITY:** High.

**RISKS:** Hard in-process guarantee is not realistically established by scattered polls.

**DO NOT DO:** Do not present a parser byte cap as an RSS cap or a callback polling point as callback preemption.

### TASK LP-01 — Sparse canonical LP solve path

**OBJECTIVE:** Remove unconditional large dense canonical allocation.

**WHY IT EXISTS:** `src/api/engine_lp.cpp` converts the working sparse canonical model with `to_dense()` before several engine paths.

**PREREQUISITES:** NUM-01 and stable sparse-basis tests.

**FILES / MODULES AFFECTED:** `src/api/engine_lp.cpp`, `include/markov_cero/lp/reference/revised_simplex.hpp`, `src/lp/reference/`, `src/lp/dual/`, `src/transform/`.

**MATHEMATICAL BASIS:** Canonical Phase I/II revised simplex, basis solves, reduced costs and rays in Section 10.

**RESEARCH REFERENCES:** Bland 1977 for anti-cycling; Forrest–Tomlin only if changing updates.

**IMPLEMENTATION STEPS:** (1) Add overload taking `SparseCanonicalModel` or a read-only CSC view. (2) Build simplex column workspace without an `m×n` dense matrix. (3) Route primal and dual dispatch through sparse overload. (4) Keep dense conversion only if `rows×columns` passes a named explicit cap. (5) Differential-test old/new results before retiring default dense path. (6) Validate original-space postsolve and certificates. (7) Profile RSS, pricing and factor time.

**ALGORITHM:** Same revised simplex algorithm; only representation/assembly changes. No mathematical method swap.

**PSEUDOCODE:** `canonical CSC → sparse columns → Phase I/II → sparse basis LU/update → witness → postsolve → original check`.

**INTERFACES:** Public `api::solve` unchanged; internal sparse overload is explicit.

**EDGE CASES:** Empty row/column, free variable split, objective offset, maximization, nearly singular basis, duplicate coefficients.

**NUMERICAL CONSIDERATIONS:** CSC duplicate aggregation and ordering must match old canonical semantics; factor residuals checked after updates.

**TESTS REQUIRED:** Existing simplex/property/warm-start/regression suite; Netlib sparse fixtures; randomly permuted equivalent models.

**BENCHMARK REQUIRED:** Matched peak RSS, wall time, iterations and factor nonzeros on frozen sparse LP set.

**DOCUMENTATION REQUIRED:** Sparse-path contract, dense adapter cap, supported size envelope.

**ACCEPTANCE CRITERIA:** No uncapped dense `m×n` allocation on public LP path; identical accepted status/objective within declared tolerances; all witnesses pass; measured RSS improvement on a sparse case.

**EXPECTED OUTPUT:** Sparse end-to-end LP path with bounded compatibility adapter.

**DEPENDENCIES:** Strengthens MILP node relaxations and later performance claims.

**ESTIMATED COMPLEXITY:** High.

**RISKS:** Basis indexing, postsolve mapping and dual sign regression.

**DO NOT DO:** Do not implement an explicit inverse or change pricing heuristics in the same PR.

### TASK MIP-01 — Certify MILP node bounds and proof obligations

**OBJECTIVE:** Prevent invalid pruning and false global claims.

**WHY IT EXISTS:** A single wrong LP relaxation bound or invalid cut can make branch-and-bound return a false optimum.

**PREREQUISITES:** NUM-01, LP-01 or an independently qualified existing node LP path.

**FILES / MODULES AFFECTED:** `src/milp/node_lp.cpp`, `src/milp/search_*.cpp`, `src/milp/gomory.cpp`, `src/milp/mir.cpp`, `src/verify/mip_proof*.cpp`.

**MATHEMATICAL BASIS:** Weak duality, exhaustive branch partition, valid inequalities and incumbent feasibility.

**RESEARCH REFERENCES:** Achterberg/Koch/Martin for branching; verified primary GMI/MIR derivations before cut changes.

**IMPLEMENTATION STEPS:** (1) Enumerate each prune reason and required witness. (2) Require finite verified node lower bound before bound pruning. (3) Prove branch children partition parent integer domain. (4) Derive cut rows under original-variable mapping and test on enumerated points. (5) Replay bounded trees, including resource stop paths. (6) Record cut/proof obligations with model fingerprint.

**ALGORITHM:** Section 11 node pseudocode; no unverified fallback becomes a proof.

**PSEUDOCODE:** `verify relaxation → prune/branch/update only from accepted witnesses → replay tree → assign assurance`.

**INTERFACES:** Preserve status/gap fields; expose proof exhaustion and assurance clearly.

**EDGE CASES:** Gap satisfied but nonzero; no incumbent; queue capacity; weak PDLP node bound; nearly integral branch value; cut rejected after re-solve.

**NUMERICAL CONSIDERATIONS:** Downward guard on lower bounds, upward guard on incumbent objective; objective sense normalized consistently.

**TESTS REQUIRED:** Exhaustive small binary/integer programs, corrupted bounds/cuts/splits, timeout with incumbent.

**BENCHMARK REQUIRED:** MIPLIB subset with status, nodes, bounds, gap and complete failures.

**DOCUMENTATION REQUIRED:** Proof scope and the difference among `Feasible`, `GapSatisfied`, `Optimal`, `Unverified`.

**ACCEPTANCE CRITERIA:** Zero accepted false proofs in preregistered adversarial suite; no unverified prune; original incumbent always independently feasible.

**EXPECTED OUTPUT:** Assured linear MILP search and proof semantics.

**DEPENDENCIES:** MIQP and MINLP master bound assurance.

**ESTIMATED COMPLEXITY:** Very high.

**RISKS:** Validity of transformed cut inequalities.

**DO NOT DO:** Do not count solver-reported node objective as a certified lower bound without its dual/support witness.

### TASK QP-01 — Qualify convex QP and PSD/KKT contracts

**OBJECTIVE:** Establish reliable convex QP status and reusable QP subproblems.

**WHY IT EXISTS:** MIQP node relaxations and SQP depend on the QP engine; an incorrect PSD or KKT result propagates upward.

**PREREQUISITES:** NUM-01, RES-01 for production qualification.

**FILES / MODULES AFFECTED:** `src/qp/model_convexity.cpp`, `src/qp/admm_solver*.cpp`, `src/qp/kkt*.cpp`, `src/qp/verifier.cpp`, `src/api/engine_qp.cpp`.

**MATHEMATICAL BASIS:** Convex QP, PSD Hessian, KKT necessity/sufficiency for convex differentiable QP under appropriate feasibility conditions.

**RESEARCH REFERENCES:** Stellato et al. OSQP paper.

**IMPLEMENTATION STEPS:** (1) Freeze Hessian convention. (2) Separate PSD/nonconvex/indeterminate. (3) Recompute KKT in original units. (4) Review primal/dual infeasibility witnesses. (5) Test repeated symbolic KKT cache after changed numeric data. (6) Measure fill/iteration behavior.

**ALGORITHM:** Existing ADMM with KKT factor; no method replacement.

**PSEUDOCODE:** `validate P → assess PSD → factor KKT → iterate ADMM under budget → verify original KKT/certificate → assign status`.

**INTERFACES:** `qp::solve_qp` and `api::solve`; result must disclose actual CPU/GPU path.

**EDGE CASES:** Zero Hessian, rank-deficient PSD, indefinite Hessian, equality bounds, empty rows, unbounded boxes.

**NUMERICAL CONSIDERATIONS:** Residual scales and termination distinct from PSD pivot tolerance; indeterminate is not PSD.

**TESTS REQUIRED:** Analytic convex QPs, rejected wrong active side/complementarity, altered multipliers, singular PSD cases.

**BENCHMARK REQUIRED:** Convex supported QPLIB subset versus OSQP/HiGHS where applicable, with original checks.

**DOCUMENTATION REQUIRED:** Convexity scope, KKT convention, stop and GPU partial-path meaning.

**ACCEPTANCE CRITERIA:** No accepted incorrect KKT in attack suite; supported-class objectives/statuses agree under declared tolerances; numerical limits explicit.

**EXPECTED OUTPUT:** Reusable qualified convex QP engine.

**DEPENDENCIES:** MIQP-01 and NLP-01.

**ESTIMATED COMPLEXITY:** High.

**RISKS:** False PSD classification and misleading ADMM convergence.

**DO NOT DO:** Do not claim general nonconvex QP or full GPU QP.

### TASK MIQP-01 — Qualify convex MIQP node bounds and proof replay

**OBJECTIVE:** Make global MIQP claims depend on valid convex QP lower bounds and complete branch coverage.

**WHY IT EXISTS:** Current MIQP uses real node QPs and supporting bounds, but a numerical overestimate could prune the true optimum.

**PREREQUISITES:** QP-01, MIP-01, NUM-01.

**FILES / MODULES AFFECTED:** `src/milp/node_qp.cpp`, `src/qp/supporting_bound.cpp`, `src/verify/mip_proof_relaxation.cpp`, `tests/qp_scenarios_1.cpp`, `tests/mip_proof_test.cpp`.

**MATHEMATICAL BASIS:** Convex supporting hyperplane and Lagrangian/box infimum; branch-and-bound lower/upper bound invariant (Section 13).

**RESEARCH REFERENCES:** Convex QP/KKT reference; branch-and-bound literature; derive the exact `supporting_lower_bound` code algebra in the new contract.

**IMPLEMENTATION STEPS:** (1) Write the lower-bound derivation for every row multiplier sign and endpoint choice. (2) Independently recompute it in a test helper using high-precision/rational tiny cases. (3) Test node overlays in both QP and support bound. (4) Verify all incumbent objectives from original quadratic data. (5) Attack proof tree records. (6) Differential-test randomized small integer boxes by enumeration.

**ALGORITHM:** Section 13 pseudocode; QP relaxation at each node and conservative support bound.

**PSEUDOCODE:** `QP KKT accepted → support bound finite and weakened → compare to incumbent → branch or prune`.

**INTERFACES:** Use existing `milp::solve`/`api::solve`; expose proof tier and gap.

**EDGE CASES:** Infinite box endpoint, maximize sign, offset, nearly integral value, PSD indeterminate, QP time limit.

**NUMERICAL CONSIDERATIONS:** Downward bound weakening must cover summation error; fail closed if finite bound cannot be established.

**TESTS REQUIRED:** Analytic MIQP examples in Section 13, randomized enumeration, altered proof/witness fields.

**BENCHMARK REQUIRED:** Separate convex MIQP stratum; do not merge with linear MIP timing summary.

**DOCUMENTATION REQUIRED:** Supported convex MIQP class, bound derivation, proof limitations.

**ACCEPTANCE CRITERIA:** Every accepted small MIQP optimum equals exhaustive optimum within tolerance; no forged bound/proof passes; incomplete nodes do not yield global status.

**EXPECTED OUTPUT:** Globally assured bounded convex MIQP subset.

**DEPENDENCIES:** Release 3 solver claim.

**ESTIMATED COMPLEXITY:** High.

**RISKS:** Sign of dual row multiplier and objective-sense conversion.

**DO NOT DO:** Do not substitute the QP primal objective for a lower bound or silently skip a node whose QP did not converge.

### TASK NLP-01 — Validate nonlinear callbacks and local SQP semantics

**OBJECTIVE:** Make NLP safe and honest as a local solver.

**WHY IT EXISTS:** Current SQP is functional but KKT alone does not prove a local minimum or global optimum; callbacks add shape, finite-value and blocking risks.

**PREREQUISITES:** QP-01, NUM-01, RES-01 for hosted use.

**FILES / MODULES AFFECTED:** `include/markov_cero/nlp/nlp_model.hpp`, `src/nlp/sqp_solver*.cpp`, `src/nlp/lbfgs.cpp`, `src/nlp/nlp_verifier.cpp`, `src/api/engine_nonlinear.cpp`, Python bindings.

**MATHEMATICAL BASIS:** First-order KKT for `g≤0,h=0`, convex QP step approximation, L-BFGS curvature and merit globalization (Section 14).

**RESEARCH REFERENCES:** Nocedal–Wright SQP chapter; local research notes are secondary.

**IMPLEMENTATION STEPS:** (1) Add checked callback adapter for dimensions, finite values and exceptions. (2) Implement optional derivative diagnostic. (3) Review bound-normal stationarity and multiplier signs. (4) Document Armijo fallback and current local step cap. (5) Preserve verified feasible point on limits. (6) Correct public assurance wording; version status schema if enum changes. (7) Profile subproblem and callback cost.

**ALGORITHM:** Existing SQP with L-BFGS and line search; no claimed second-order guarantee.

**PSEUDOCODE:** Section 14 pseudocode.

**INTERFACES:** `nlp::solve_sqp`, callback binding, `api::solve`; expose `local_kkt_only` assurance and callback evaluation count.

**EDGE CASES:** Wrong-sized gradient/Jacobian; NaN/throw/stall; infeasible QP linearization; saddle; active bounds; objective maximization bridge.

**NUMERICAL CONSIDERATIONS:** Finite differences are diagnostic only; KKT scaling and bound normals must use original units; line-search failure is inconclusive.

**TESTS REQUIRED:** All analytic/adversarial NLP cases in Section 14, from multiple starts where relevant.

**BENCHMARK REQUIRED:** Separate local NLP corpus; compare feasibility/stationarity/time, not global objective proof.

**DOCUMENTATION REQUIRED:** Callback contract, local guarantee, unsupported globality, options and failure meanings.

**ACCEPTANCE CRITERIA:** Wrong derivatives/invalid callbacks are diagnosed; no saddle is described as a verified local minimum; accepted local KKT points pass independent original-space checks.

**EXPECTED OUTPUT:** Qualified local SQP capability.

**DEPENDENCIES:** MINLP fixed-integer NLP subproblems.

**ESTIMATED COMPLEXITY:** High.

**RISKS:** Inconsistent multipliers and nondifferentiable/unstable callbacks.

**DO NOT DO:** Do not call SQP failure a proof of infeasibility or KKT a global proof.

### TASK NLP-02 — Restoration for inconsistent SQP linearizations

**OBJECTIVE:** Improve NLP robustness only after NLP-01 failure cases are measured.

**WHY IT EXISTS:** A nonlinear feasible model may yield an infeasible linearized QP from a poor iterate; repeated Hessian reset does not necessarily restore feasibility.

**PREREQUISITES:** NLP-01, QP-01 and a frozen failure corpus showing this specific issue.

**FILES / MODULES AFFECTED:** `src/nlp/sqp_solver_subproblem.cpp`, `src/nlp/sqp_solver.cpp`, contract/tests.

**MATHEMATICAL BASIS:** Elastic SQP subproblem introduces nonnegative violation slacks penalized in the objective; a restoration phase reduces original constraint violation without claiming objective progress.

**RESEARCH REFERENCES:** Nocedal–Wright SQP/restoration discussion; extract exact penalty and acceptance rules before coding.

**IMPLEMENTATION STEPS:** (1) Minimize a failing feasible example. (2) Specify elastic variables and penalty update. (3) Solve bounded convex QP restoration step. (4) Accept only when original nonlinear violation decreases by a documented amount. (5) Return inconclusive after bounded failures. (6) Compare against old path.

**ALGORITHM:** Elastic restoration, not a second general nonlinear engine.

**PSEUDOCODE:** `QP infeasible → build slack-penalized QP → solve → check original violation decrease → resume SQP or stop`.

**INTERFACES:** Internal; expose restoration count/reason in diagnostics.

**EDGE CASES:** Truly infeasible model, zero step, variable bound corner, no finite merit decrease.

**NUMERICAL CONSIDERATIONS:** Slack penalty cannot justify infeasibility or optimality; verify original constraints after each step.

**TESTS REQUIRED:** One feasible-but-linearization-infeasible case; one truly infeasible case remains inconclusive; regression of existing local solutions.

**BENCHMARK REQUIRED:** Paired robust-convergence rate and overhead on frozen NLP cases.

**DOCUMENTATION REQUIRED:** Restoration contract and limits.

**ACCEPTANCE CRITERIA:** Measured failure class improves without a false success or material regression.

**EXPECTED OUTPUT:** Optional, evidence-backed SQP robustness improvement.

**DEPENDENCIES:** Helpful to MINLP; not required to assert basic local NLP.

**ESTIMATED COMPLEXITY:** High.

**RISKS:** Penalty tuning and infinite restoration loops.

**DO NOT DO:** Do not implement until a minimized failure case and mathematical specification exist.

### TASK MINLP-01 — Validate convex quadratic OA cuts and master bounds

**OBJECTIVE:** Make restricted convex MINLP OA mathematically defensible.

**WHY IT EXISTS:** Current OA loop generates tangents and uses a MILP master, but the global result is solver-trusted and no independent OA replay is exported.

**PREREQUISITES:** NLP-01, MIP-01, QP-01, NUM-01.

**FILES / MODULES AFFECTED:** `src/minlp/minlp_solver.cpp`, `src/minlp/minlp_solver_iterate_outer_approximation.cpp`, `src/io/nlp_callbacks.cpp`, `src/api/engine_nonlinear.cpp`, `tests/minlp_basic_test.cpp`.

**MATHEMATICAL BASIS:** Convex tangent underestimators, OA master relaxation and verified incumbent upper bound (Section 15).

**RESEARCH REFERENCES:** Duran–Grossmann and Bonami et al. cited in Section 24.

**IMPLEMENTATION STEPS:** (1) Freeze supported structural class and 512-variable screen. (2) Derive/test combined QUADOBJ/NLOBJ Hessian and maximize sign. (3) Store tangent provenance. (4) Independently recompute/verify each generated tangent on random feasible points and analytic cases. (5) Accept fixed-integer NLP solution as incumbent only after original nonlinear feasibility/integrality/objective recomputation. (6) Accept master lower bound only if MILP layer certifies it; handle incomplete/unbounded master honestly. (7) Add caps for OA rows/iterations and counters.

**ALGORITHM:** Convex outer approximation alternating fixed-integer NLP and MILP master.

**PSEUDOCODE:** Section 15 pseudocode.

**INTERFACES:** Preserve `minlp::solve_minlp`; expose bound provenance and solver-trusted versus independently replayed assurance.

**EDGE CASES:** Nonconvex or indeterminate Hessian, nonlinear equality, callback-only source, unbounded master, SQP failure, maximization/offset, finite/infinite epigraph bound.

**NUMERICAL CONSIDERATIONS:** Numerically weaken cuts outward; a rounded tangent that excludes a feasible integer point is unsound. Compare bound `L≤U` with a documented error allowance.

**TESTS REQUIRED:** Section 15 analytic cases; exhaustive small integer assignments with continuous subproblem oracle; corrupted tangent tests.

**BENCHMARK REQUIRED:** Separate restricted convex MINLP stratum; report SQP calls/failures, master nodes/cuts, valid bound and gap.

**DOCUMENTATION REQUIRED:** Exact structural scope, OA derivation, solver-trusted limitation.

**ACCEPTANCE CRITERIA:** No invalid tangent in analytic/random validation; no false global assurance; all accepted incumbents original-feasible.

**EXPECTED OUTPUT:** Honest restricted convex MINLP prototype with validated cuts and bounds.

**DEPENDENCIES:** MINLP-02 for independent global certification.

**ESTIMATED COMPLEXITY:** Very high.

**RISKS:** Objective convention, tangent sign and invalid master lower bound.

**DO NOT DO:** Do not allow arbitrary callbacks into global OA without checkable convexity; do not add big-M to hide unbounded masters.

### TASK MINLP-02 — Independent OA proof replay

**OBJECTIVE:** Make a global convex MINLP claim independently checkable.

**WHY IT EXISTS:** `canonical_verified=false` currently records that the OA bound is not independently replayed.

**PREREQUISITES:** MINLP-01 and MIP-01 proof framework.

**FILES / MODULES AFFECTED:** New bounded OA proof structures under `include/markov_cero/verify/` and `src/verify/`; `src/minlp/`, `src/api/engine_nonlinear.cpp`, CLI proof tool and tests.

**MATHEMATICAL BASIS:** Valid convex tangents, exhaustive master MIP tree, feasible incumbent and `L≤OPT≤U`.

**RESEARCH REFERENCES:** Duran–Grossmann/Bonami et al. assumptions and an independently reviewed proof specification.

**IMPLEMENTATION STEPS:** (1) Write proof format version and exact obligations before serialization. (2) Record source model hash, convexity evidence, every tangent point/function/gradient, every master revision, node bound witness and incumbent. (3) In a separate replay entry point, rebuild tangents from source polynomial and verify cut directions with numerical weakening. (4) Replay the MILP master proof and monotone bound history. (5) Verify final incumbent and gap. (6) Enforce proof size/time/node caps. (7) Fuzz serialized proof and mutation-test every obligation.

**ALGORITHM:** Deterministic verifier, never an optimization fallback.

**PSEUDOCODE:** `check fingerprint/structure → recompute convexity/tangents → replay master bounds/tree → check incumbent → check gap → accept or reject/exhaust`.

**INTERFACES:** New versioned proof object/CLI; `canonical_verified=true` only on accepted replay.

**EDGE CASES:** Missing tangent, altered gradient, stale model hash, truncated tree, proof budget exhaustion, maximize sign, repeated cut.

**NUMERICAL CONSIDERATIONS:** Proof uses independent recomputation and conservative inequality guards; state explicitly that floating-point replay is not a formal exact proof.

**TESTS REQUIRED:** Positive proof fixtures and one mutation per proof field; random small convex quadratic MINLP enumeration.

**BENCHMARK REQUIRED:** Proof time/size overhead and accepted fraction on frozen MINLP suite.

**DOCUMENTATION REQUIRED:** Assurance limitations, proof version and replay command.

**ACCEPTANCE CRITERIA:** No forged proof accepted in preregistered attacks; exhausted proof leaves global status unverified; independent reviewer signs off on inequalities.

**EXPECTED OUTPUT:** Bounded, independently replayable convex MINLP proof tier.

**DEPENDENCIES:** Required for verified global MINLP product claim.

**ESTIMATED COMPLEXITY:** Very high; may be deferred beyond SIH.

**RISKS:** Verifier sharing too much code with generator and therefore reproducing its bug.

**DO NOT DO:** Do not set `canonical_verified=true` merely because the solver's own OA gap is small.

### TASK BENCH-01 — Frozen differential and performance campaign

**OBJECTIVE:** Establish supported-class correctness and measured scale without cherry-picking.

**WHY IT EXISTS:** Historical comparisons are useful but do not prove the current committed binary's breadth or speed.

**PREREQUISITES:** BASE-01 and the domain gate whose claim is being measured.

**FILES / MODULES AFFECTED:** `scripts/run_*`, `scripts/support/`, `data/` manifests, `evidence/` raw outputs.

**MATHEMATICAL BASIS:** Independent original-model feasibility/objective checking and class-specific assurance.

**RESEARCH REFERENCES:** Official Netlib/MIPLIB/QPLIB collections linked in Section 20.

**IMPLEMENTATION STEPS:** (1) Preregister exact supported subset, splits and caps. (2) Hash model/binary/comparator. (3) Execute matched process runs with equal threads and at least five repeats for speed promotion. (4) Keep unsupported/timeout/crash rows. (5) Independently check returned primals. (6) Compute solved fraction, status/objective disagreement, paired time distributions, peak RSS, nodes and gaps. (7) Reproduce conclusions on another host.

**ALGORITHM:** Deterministic harness orchestration; no oracle called by runtime solver.

**PSEUDOCODE:** `manifest → run each solver/instance/repeat → check original model → append immutable row → summarize all rows`.

**INTERFACES:** Stable CSV/JSON schema with manifest version.

**EDGE CASES:** Comparator parses a different model; model unsupported by one solver; parent watchdog kills process; no feasible point returned.

**NUMERICAL CONSIDERATIONS:** Compare objectives only after checking class, sense and original feasibility; use declared absolute/relative tolerance.

**TESTS REQUIRED:** Harness self-test with fabricated timeout/parse failure/objective disagreement records; no fake benchmark outcomes.

**BENCHMARK REQUIRED:** Netlib LP, MIPLIB, convex QPLIB, separate MIQP/local NLP/convex MINLP strata.

**DOCUMENTATION REQUIRED:** Hardware, software, models, caps, raw logs, statistical method and limitations.

**ACCEPTANCE CRITERIA:** Full denominator, reproducible manifest and second-host validation for promoted claims.

**EXPECTED OUTPUT:** Evidence-backed correctness and performance report.

**DEPENDENCIES:** Release claim and optimization prioritization.

**ESTIMATED COMPLEXITY:** High compute and engineering effort.

**RISKS:** Timing inequivalence and benchmark selection bias.

**DO NOT DO:** Do not remove failed rows or compare local NLP objective to a claimed global optimum as proof.

### TASK REL-01 — Package and deploy the supported solver

**OBJECTIVE:** Produce a maintainable release and optional safely bounded service.

**WHY IT EXISTS:** Algorithmic correctness alone does not make a usable product.

**PREREQUISITES:** BASE-01, RES-01, supported-domain gates and BENCH-01.

**FILES / MODULES AFFECTED:** CMake install/package, `pyproject.toml`, CI, Dockerfile, `web/backend/server.py`, release docs.

**MATHEMATICAL BASIS:** No new algorithm; status and assurance contracts must survive packaging/serialization.

**RESEARCH REFERENCES:** Platform/packaging/security documentation at implementation time.

**IMPLEMENTATION STEPS:** (1) Publish supported OS/compiler/Python matrix. (2) Build wheels/CMake package cleanly. (3) Run installed external consumer. (4) Generate SBOM and source/binary hashes. (5) Run hosted sanitizer/CUDA compile evidence as claimed. (6) Add service OS process limits, request quotas, metrics and rollback. (7) Name support/security owners. (8) Get independent release review.

**ALGORITHM:** Release pipeline, not optimizer logic.

**PSEUDOCODE:** `tag committed source → clean build matrix → test → hash/sign → install consumer → service smoke/abuse → publish supported scope`.

**INTERFACES:** Versioned JSON/result schema; documented C++/Python compatibility.

**EDGE CASES:** Missing CUDA compiler/device, missing optional ML model, malformed remote input, worker crash, result larger than response cap.

**NUMERICAL CONSIDERATIONS:** Re-run mathematical corpus on shipped binaries, not only source-tree debug builds.

**TESTS REQUIRED:** Hosted CI, sanitizers, installed consumer, wheel tests, worker kill and service abuse/limit cases.

**BENCHMARK REQUIRED:** Shipped binary replays frozen representative performance suite; no unexplained major regression.

**DOCUMENTATION REQUIRED:** Install, API, limitations, support, security contact, rollback and release notes.

**ACCEPTANCE CRITERIA:** Independent user installs and reproduces results on a second host; all release artifacts trace to one commit and license; service limits hold under abuse tests.

**EXPECTED OUTPUT:** Supported package and optional bounded service.

**DEPENDENCIES:** Final production release.

**ESTIMATED COMPLEXITY:** High operational effort.

**RISKS:** ABI/platform drift and unowned incidents.

**DO NOT DO:** Do not advertise GPU, ML, NLP or MINLP as production supported unless their separate gates close.

## 29. Acceptance Criteria and Stop Conditions

A task is complete only when the mathematical contract, implementation diff, tests, benchmark, and documentation refer to the **same committed revision**. Mathematical correctness outranks a performance win. Required stop-and-investigate events:

1. A deliberately false certificate or invalid cut is accepted.
2. An output status changes without an explained mathematical or tolerance change.
3. A new path has a larger original-space residual than the declared acceptance limit.
4. A branch node is pruned without a verified valid lower bound.
5. SQP reports a global claim or a verified local minimum solely from first-order KKT.
6. OA reports global certification without independently checked convexity, cuts, master bound and tree coverage.
7. Peak RSS/deadline violates the advertised resource envelope.
8. A benchmark improvement disappears on the held-out set or second host.

On a stop: minimize the input, save source/binary/model hashes, identify the violated invariant, add a regression test, fix the root cause, rerun the campaign slice, then continue. Do not stack another feature on the unresolved failure.

## 30. Release Plan

| Release | Honest claim | Required gates |
|---|---|---|
| R0 research prototype | Current implementation, explicitly scoped | Current tests and status page |
| R1 stable LP core | Supported LP with verified primal/dual/ray statuses and sparse path | BASE-01, NUM-01, LP-01, relevant RES-01 |
| R2 robust LP + convex QP | Presolve/scale/dual and QP qualified | Presolve contract and QP-01 |
| R3 MILP + convex MIQP | Valid incumbents, bounds and bounded replay | MIP-01, MIQP-01 |
| R3a local NLP preview | Local KKT candidate only | NLP-01; optional NLP-02 |
| R3b restricted convex MINLP preview | Verified incumbent; OA bound labelled solver-trusted | MINLP-01 |
| R4 production package | Stable API, resource envelope, hosted CI, support | BENCH-01, REL-01 |
| R5 industry-grade declared niche | Independent reproduced robustness/performance and release maintenance | Broader holdout, second host, support track record; MINLP-02 if global MINLP is claimed |

Release numbers are qualification levels, not an instruction to bump package versions mechanically.

## 31. SIH Readiness Milestones

1. Offline build and solve with external oracles removed from runtime environment.
2. Show an LP optimum with primal/dual gap and one infeasible or unbounded witness.
3. Show a MILP and MIQP with incumbent, valid bound, gap and proof tier; deliberately trigger proof exhaustion.
4. Show convex QP KKT residual and a nonconvex input rejected.
5. Show NLP Rosenbrock as a **local** result and the saddle counterexample to explain the limitation.
6. Show convex quadratic MINLP OA with a verified incumbent, master bound and visibly *solver-trusted* global assurance unless MINLP-02 is complete.
7. Show a timeout retaining feasible work without declaring optimum.
8. Show a complete comparison table including slower and failed cases.
9. Show exact implemented/experimental/unsupported labels. Public refinery models remain qualification examples, not plant validation.

## 32. Industry-Grade Definition of Done

A clean source release solves a **declared** LP/MILP/convex-QP/convex-MIQP envelope with independently checked accepted statuses; maintains numerical behavior on pathological and recognized suites; has a measured resource envelope and hard hosted-process caps; reports a complete benchmark denominator and second-host reproduction; ships coherent licensed packages, API docs, SBOM and named maintainers. NLP additionally has honest local first-order semantics and callback safety. Global convex MINLP is included only if OA proof replay is independently accepted. Operational refinery use requires separate engineer-approved formulation/data and a supervised shadow pilot.

## 33. Biggest Technical Risks

1. False global proof from an LP/MILP/MIQP node bound or cut.
2. OA tangent or master bound invalid under objective sense, quadratic convention or floating error.
3. SQP first-order KKT mislabelled as a local minimum/global optimum.
4. Dense LP conversion and sparse factor fill exhausting memory.
5. Callback or factorization duration exceeding cooperative deadlines.
6. Historical evidence attributed to a newer binary.
7. Licensing/support ambiguity preventing a responsible release.
8. Feature proliferation delaying verification of the core.

## 34. Things We Explicitly Should Not Build Yet

No new model hierarchy, plugin system, registry, distributed scheduler, database, Kubernetes cluster, general nonconvex MIQP/MINLP global algorithm, full-device QP claim, new cut family, Devex/steepest-edge rewrite, trained ML branching promotion, or refinery operational integration without a measured need and mathematical contract. Keep experimental code in-tree where useful, but do not silently promote it into the supported release configuration.

## 35. Recommended Development Order and Final Engineering Strategy

```text
BASE-01
  → NUM-01 and RES-01
  → LP-01 and QP-01
  → MIP-01
  → MIQP-01
  → NLP-01 (NLP-02 only after a measured restoration failure)
  → MINLP-01 (MINLP-02 only for a certified global MINLP claim)
  → BENCH-01
  → REL-01
```

Build one mathematically defensible slice at a time. Keep original-model verification outside the engine that generated the point. Make every limit and every assurance tier visible. Benchmark a frozen binary after correctness, then optimize the measured bottleneck. The result should be a small, maintainable solver with a trustworthy supported envelope rather than a large collection of partially validated algorithms.
