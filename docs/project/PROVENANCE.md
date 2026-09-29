# Provenance and source exposure

This record explains what can be attributed to checked-in inputs and what still
requires independent review. It is a history of claims and implementation
episodes, not a certificate of source independence. The current
[capability register](STATUS.md) and [evidence index](../../evidence/INDEX.md)
should be used for release and performance statements; dated test counts below
belong to the source states that produced them. The markov-cero team has not
recorded an independent line-by-line source-trace sign-off.

Historical audit paths cited below were removed from the current checkout during
repository cleanup. Their dated contents remain available in Git history.

- Baseline: empty original repository created for markov-cero M0 on 2026-09-13.
- M0 implementation baseline: repository created 2026-09-13. Historical project records describe that implementation as independent and state that no external solver implementation source was consulted during that coding period. Those records have not been independently audited against access history or a line-by-line code trace.
- Later source exposure: the competitive-landscape audit states that 27 peer solver repositories were cloned and inspected locally on 2026-09-25 for line-level verification (`docs/audit/19-competitive-landscape.md`, §§19.0, 19.9). This is source-level exposure and supersedes any unqualified statement that competitor code was never inspected during the project.
- Purpose recorded for that later inspection: retrospective competitive analysis. The repository does not yet contain an independent trace review establishing whether or not that exposure influenced implementation.
- Inputs recorded for the original implementation: project requirements, independently written mathematical statements, public bibliographic metadata, and sanitized behavior-level observations. The completeness of this historical record is unverified.
- Production external-solver path: prohibited and absent. This dependency-policy fact does not establish clean-room provenance.

Clean-room status: **not independently verified; do not make an unqualified clean-room claim**. Before a release claim, an independent reviewer must inspect the dated source-exposure record, relevant Git history/diffs, and algorithm-level similarities; document findings and quarantine/reimplement any affected work if required by the approved SIH policy. Lack of evidence of copying is not evidence that no influence occurred.

Each later change must state sources consulted, source exposure, derivation references, affected invariants, and reviewer status. If a review identifies a contaminated component, quarantine it and reimplement from approved requirements and independently derived mathematics.

## Implementation record (2026-09-28)

- Change: expose finite MIP proof budgets through the C++ API, solve CLI, Python interface and standalone verifier; add proof-build and proof-replay timing to result envelopes.
- Sources consulted for this change: in-repository implementation, tests and `docs/audit/INDUSTRY-GRADE-COMPETITIVE-ROADMAP.md`. No external solver source or peer repository was consulted during this change.
- Derivation/invariant: the existing proof contract bounds both generation and replay; both stages must remain inside the configured proof deadline and the overall solve deadline. Telemetry reports stage elapsed time and does not alter acceptance criteria.
- Validation: complete `build_readiness` target set built; CTest passed 80/80; source limit check found 0 violations; documentation link check found 0 broken links; `git diff --check` passed.
- Limitation: the separate Python extension was not built because `pybind11` is unavailable in the current Python environment; its edited binding code remains unverified by compilation or runtime tests.
- Reviewer: no independent code or mathematical review recorded yet.

## Implementation record: persistent node state (2026-09-28)

- Change: replace per-node lower/upper bound vectors with immutable parent-linked deltas in `node_bounds.hpp`, inherited local-cut vectors with copy-on-write lists in `node_cuts.hpp`, and copied warm-start bases with shared immutable basis pointers; serial and parallel search reuse bound-materialization path buffers; parallel workers pass effective bounds as LP/QP overlays, avoiding a full worker model copy in revised-simplex and QP paths. Add regressions for scratch-capacity reuse, sibling basis sharing and tightened QP bounds/supporting lower bound.
- Sources consulted: in-repository MILP node, queue, worker and test code. No external solver source or peer repository was consulted for this change.
- Invariant: each node's effective domain is the root domain intersected with every bound delta along its ancestry; sibling deltas and inherited cut lists cannot mutate one another. Siblings reference the same immutable warm-start basis; active simplex invocation receives a value copy. Relaxation, rounding and QP lower-bound calculations use effective node bounds. Materialization scratch is worker-local and reused sequentially, never shared across concurrent workers.
- Validation: full `build_readiness` native build succeeded; CTest passed 80/80 after the scratch-reuse change; ten focused MILP/parallel/QP CTests passed, including 256 deterministic randomized bound-reconstruction cases, scratch-capacity reuse, sibling cut-list and basis sharing checks, and a tightened QP-bound case. `scripts/check_source_limits.py` checked 422 maintained files with 0 violations; `scripts/check_docs.py` checked 8 active documents with 0 broken local links; `git diff --check` passed.
- Memory evidence: `scripts/bench_node_frontier_memory.cpp`, built with GCC 16.2.1 in Debug, compared 500–4,000 structure-only queued nodes. At 4,000 nodes, Linux peak RSS was 169,952 KiB for the materialized baseline and 6,792 KiB for persistent state; exact parameters and caveats are in `evidence/node-frontier-memory-20260928.json`. This does not measure full solver RSS.
- Remaining scope: queued children no longer each own two full bound arrays, a copied local-cut list, or duplicate basis vectors. Root and serial mutable `Model` workspaces, large-PDLP effective-model materialization, per-node cut application, basis copy at pop, bounded persistent-structure policy, and full-solver frontier/RSS scaling evidence remain. IR-19 remains open.
- Reviewer: no independent code or mathematical review recorded yet.

## Implementation record: queued-node resource cap and bound propagation (2026-09-28)

- Change: add a validated queued-node cap to serial and parallel MILP options, CLI and Python APIs; return `ResourceLimit` on exhaustion while retaining a conservative bound for omitted frontier nodes; fix parallel search to propagate certified LP lower bounds instead of primal relaxation objectives; add explicit LP parser limits for bytes/tokens/rows/columns/coefficients/quadratic terms/names; expose a per-file byte override through `solve_file`, CLI and Python; classify MPS/LP cap exhaustion as `ResourceLimit` with a dedicated input-resource diagnostic; split affected parser/search/test modules to satisfy the 300-line source limit.
- Sources consulted: in-repository search, queue, result, API and test code plus the competitive roadmap. No external solver code or peer repository was consulted during this change.
- Mathematical invariant: for minimization, each LP relaxation lower bound is a lower bound on every integer descendant; incumbent objectives are upper bounds and must not be substituted for those bounds. If a frontier node is omitted at queue capacity, the minimum inherited certified bound of all omitted nodes must participate in the global lower bound. A capped tree cannot establish infeasibility or optimality merely because the retained queue empties. Parallel child insertion is atomic for sibling pairs so a branch does not silently retain only one side.
- Validation for the queue-cap/bound work: the Python extension passed 18/18; a complete native build and CTest passed 81/81 in 122.53 seconds. LP parser limits were then added with negative tests for byte, token, row, column, coefficient, quadratic-term and file-streaming caps. The parser test passed after rebuilding; the isolated supply-chain integration case passed in 56.96 seconds. A fresh complete native build and CTest then passed 81/81 in 131.78 seconds after setting that integration case's solver limit to 90 seconds (120-second outer timeout) to tolerate observed suite-load variance. The final static checks are source limits (425 files, max 300, zero violations), documentation links (8 docs, zero failures) and `git diff --check`, all passing.
- Limitation: the configured limit counts queued nodes, not bytes. It does not bound parser memory, sparse-factor fill, cut pools, proof ledgers, allocation failures or GPU memory, and process isolation/recovery is not implemented. Queue-node omission semantics and tested solver families do not establish an end-to-end memory budget. IR-20/21 remain open.
- Reviewer: no independent code or mathematical review recorded yet.

## Public refinery benchmark (2026-09-28)

The numeric input in `data/refinery/fawley_public.json` was transcribed from
[GAMS FAWLEY model 65](https://www.gams.com/latest/gamslib_ml/libhtml/gamslib_fawley.html),
which cites Palmer, *A Model Management Framework for Mathematical Programming* (1984).
The published modeling formulation was read. The Python generator was independently implemented from
these data and balance equations. This is a historical illustrative model, not
MRPL data or an engineer-approved operating model. Its RON and viscosity rules are
approximate; lead cost and fuel-equivalent transfers retain historical semantics.
Fuel-equivalent quantities must not be interpreted as conserved physical mass.

Input units and source metadata are embedded in the JSON; the generated MPS embeds
its input SHA-256. The dataset is attributed external material, not a claim of
ownership or a grant of rights by this project. Redistribution rights and plant
engineering approval remain review gates. HiGHS is used only as a development
comparison oracle and is not linked to the production solver.
