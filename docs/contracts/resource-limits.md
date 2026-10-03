# Resource limits contract — library (v1)

Binding for the C++/Python library API. It defines how a solve stops when it
runs out of time, memory, quota or input capacity, and exactly what a result
must say about that stop. The hosted web service adds OS-level hard limits
(CPU, address space, files, output, wall clock) that are enforced outside the
library; that contract is [hosted-limits.md](hosted-limits.md).

Related contracts and evidence:

- [Numerical contract](numerical-policy.md) — tolerances, statuses,
  verification boundaries and assurance labels.
- [Hosted limits contract](hosted-limits.md) — kernel-enforced CPU, memory,
  file, output and wall limits for each hosted solve child.
- `evidence/resource-envelope-20260928.json`, `evidence/resource-instrumentation-20260928.json`
  — measured deadline/memory behavior (IR-20/IR-21).
- `evidence/resource-overrun-rss-20260930.json` — measured wall-overrun and
  RSS distributions plus the empirical polling bound (2.3 ms worst case on
  the recorded inputs).
- `evidence/device-memory-budget-2026-10-03.json` — device-buffer metering:
  budget semantics, boundary mapping and host/device solve behavior.
- `evidence/peak-memory-envelope-2026-10-03.json` — measured solve budget
  ladder, adversarial factor-fill contrast and published peak RSS (IR-21).

## 1. The cooperative stop model

One `core::SolveContext` is created per solve at the API boundary
(`src/api/api.cpp`). It carries the solve-wide deadline, the instrumented
memory budget, thread/device quota, cancellation and the **first recorded stop
reason**. Reasons are sticky: the first observed reason wins so a later reason
can never overwrite the explanation of why work stopped.

Device (GPU) buffers are metered beside that context: when `backend == "gpu"`,
`run_engine` installs a solve-scoped `gpu::DeviceBudget` on the solving thread
for the engine phase. It accounts every byte admitted through the gpu buffer
layer (live-outstanding — charged on allocation, released on free) and refuses
charges beyond `device_memory_limit_bytes`. A refusal propagates to the API
boundary, which records `device_memory_budget_exhausted` exactly like any other
boundary stop; unset keeps the accounting and disables the refusal, mirroring
`memory_limit_bytes`.

Invariants on every path (W02/D06):

1. A recorded stop never yields `optimal`, `infeasible` or `unbounded`.
2. A `resource_limit` result is never `verified` and its `assurance` is
   derived after the stop is applied (`api::detail::finalize`).
3. `SolveResult::stop_reason` is a machine-readable string copied from the
   first recorded reason (CLI JSON `stop_reason`, Python `stop_reason`).

## 2. Stop reasons

| `StopReason` | Reported string | Recorded by | Meaning |
|---|---|---|---|
| `none` | `none` | — | No stop recorded. It does not assert optimality, only that nothing stopped the solve. |
| `deadline_exceeded` | `deadline_exceeded` | context poll, engine deadline notes, serial MILP time limit | The solve-wide (or engine, for engine-local limits) wall clock ran out. |
| `cancelled` | `cancelled` | `SolveContext::request_cancel` | The caller cancelled the solve. |
| `memory_budget_exhausted` | `memory_budget_exhausted` | instrumented charge refusal | An instrumented solver-owned charge exceeded `memory_limit_bytes`. |
| `queue_capacity_exhausted` | `queue_capacity_exhausted` | parallel search | The queued-node capacity for a parallel search filled. |
| `quota_exhausted` | `quota_exhausted` | serial MILP node cap, parallel quotas | An engine quota (nodes/iterations) was reached. |
| `input_limit` | `input_limit` | API boundary | A configured parser byte/structural cap rejected the input before the solve started. |
| `work_limit` | `work_limit` | API boundary | A dimension/factor/fill limit (`std::length_error` escaping an engine) stopped the work. |
| `allocation_failure` | `allocation_failure` | API boundary | Host allocation failure (`std::bad_alloc`) mapped to a resource outcome, never a numerical one. |
| `device_memory_budget_exhausted` | `device_memory_budget_exhausted` | API boundary (device budget refusal) | A device-buffer charge exceeded `device_memory_limit_bytes`; the gpu layer refused the charge before the device allocator ran. |

Additive: new reasons append to the enum; existing reported strings and their
meaning do not change.

## 3. Attribution rules

- **R1 — sticky.** The first recorded reason wins (`SolveContext::note_stop`).
- **R2 — completeness.** Every result with `status == resource_limit` carries a
  **non-empty** `stop_reason`. Nothing else ever sets the field.
- **R3 — precise at the boundary.** When the API boundary can attribute the
  stop, it records the exact reason: parser caps → `input_limit`, escaped
  `std::length_error` → `work_limit`, `std::bad_alloc` → `allocation_failure`,
  expired deadline → `deadline_exceeded` (including the serial MILP engine
  time limit), node quota → `quota_exhausted`, charge refusal →
  `memory_budget_exhausted`, device-buffer charge refusal →
  `device_memory_budget_exhausted`, device allocator failure
  (`gpu::DeviceAllocationFailure`, a `std::bad_alloc`) → `allocation_failure`
  with the device-specific diagnostic fields.
- **R4 — honest fallback.** When a `resource_limit` result reaches the boundary
  with no recorded reason (an engine-internal stop the boundary cannot see,
  such as a serial queued-node capacity stop), `stop_reason` is set to
  `unspecified_resource_limit`. The boundary never guesses between candidate
  reasons; the engine's `message` carries the detail.
- **R5 — empty only off the resource path.** `stop_reason` is empty for
  `optimal`, `infeasible`, `unbounded`, `iteration_limit`, `invalid_model`,
  `invalid_options`, `unsupported` and every other status that no stop is
  attached to.

## 4. What this contract does not claim

- **Polling is cooperative, not preemption.** The shared context is checked at
  documented phase boundaries (`stop_after_deadline` in
  `src/api/api_internal.hpp`: parsing/dispatch, model hashing, classification,
  canonicalization, presolve, scaling, LP solve, postsolve,
  PDLP, QP, NLP/MINLP, MILP search and verification, parallel search and
  verification, MIP proof build and replay) plus a post-engine completion sweep
  around row-activity computation in `src/api/dispatch.cpp`, and inside a few
  kernels that accept a deadline (presolve passes, sparse LU numeric updates
  every 64 steps, LP/QP engine loops). The serial MILP search additionally
  polls the shared context at its root and per-node boundaries
  (`src/milp/milp_solver.cpp`). Sections without a deadline parameter — file
  parsing (bounded instead by `maximum_input_bytes`), hash/classification
  internals, canonicalization internals, JSON serialization and result
  finalization — run to completion.
- **`memory_limit_bytes` is not an RSS ceiling.** It meters instrumented
  solver-owned charges only, not every glibc or third-party allocation; it
  cannot promise a resident-set limit or suppress host OOM
  (`evidence/resource-envelope-20260928.json`).
- **`maximum_input_bytes` is not an RSS cap.** It bounds parser input before
  tokenization; it says nothing about resident memory.
- **Charges are explicit estimates, not allocator telemetry.** Instrumented
  charge points include the canonical working model, proof
  build, NodeView materialization, and frontier nodes — `sizeof(BranchNode)`
  per pushed node, the same estimate on the serial and parallel searches —
  plus the factor workspaces charged live-outstanding (admitted when the
  factor is built, released when it dies through rebuild, scope end, move or
  session cache reset): sparse basis-factor fill in the reference simplex, the
  dual session (including across-solve cache rebinds), node-relaxation LPs,
  and the sparse QP KKT factor workspace in both the root QP and node QPs,
  and the retained result vectors at finalization (best-effort: a refused
  retained charge never rewrites an already-completed result).
  `memory_charged_peak_bytes` reports the high-water mark of admitted
  instrumented charges. Factor fill is still bounded by its fill cap — an
  over-full factorization throws `std::length_error` before any partial
  factor exists and surfaces as `work_limit` — and is additionally charged to
  `memory_limit_bytes`; a refused charge stops the factorization with
  `memory_budget_exhausted` instead of admitting nothing and running. Three
  classes stay outside the charge and are bounded by their own caps instead:
  interior-point and PDLP crossover/normal-equation factors (one-shot, fill-
  capped), strong-branching and heuristic LP solves (no solve context reaches
  them), and NLP solver workspaces (dimension-capped).
- **`peak_rss_bytes` is a measurement, not a ceiling.** Every result
  publishes the process resident-set high-water mark sampled once at
  finalization (`/proc/self/status` `VmHWM` on Linux, `getrusage` on macOS,
  0 elsewhere). It covers the whole process — parse, canonicalization and
  solve — never refuses work, and says nothing about memory held outside the
  process.
- **`device_memory_limit_bytes` meters the gpu buffer layer, not all device
  memory.** It bounds bytes admitted through `gpu::DeviceBuffer` while the
  solve-scoped budget is installed (the engine phase of a `backend == "gpu"`
  solve), accounted on the solving thread; it does not meter CUDA contexts,
  streams, module images or another process's device memory, and it never
  consults `cudaMemGetInfo`, so it can refuse before the device is physically
  full. `device_memory_charged_peak_bytes` is the high-water mark of admitted
  device-buffer bytes for the solve; it is 0 when no device budget was
  installed. A hard device allocator failure is reported as `allocation_failure`
  (the shared allocation reason; the boundary does not distinguish host from
  device there) with `failure_site = "device_allocation_failure"` and a
  device-oriented suggested recovery.
- **Native limits stay cooperative** unless a complete allocator contract is
  proven; OS-level hard limits are the hosted service's contract
  ([hosted-limits.md](hosted-limits.md)), not the library's.

## 4b. Measured deadline overrun envelope (IR-20, 2026-10-03)

End-to-end deadline behavior is measured through the production CLI by
`scripts/deadline_envelope.py` and recorded in
`evidence/deadline-envelope-2026-10-03.json`: **41 runs** — 8 engines
(primal, dual, ipm, pdlp, qp, miqp, milp, parallel) under already-expired
(1e-6 s), tiny and short limits on five checked-in models, plus an
expiry-landing sweep of 11 fine-grained limits that scatter the expiry
across parse, canonicalization, presolve, root/cut/factorization and early
search units. Every expired run reported `ResourceLimit` +
`deadline_exceeded`, no terminal status was claimed past its deadline, and
whole-process wall-clock overrun (process start, parse, cooperative stop,
result finalization) stayed at or below **9.1 ms**. The unit-layer
companion in `tests/readiness_edge_cases_test.cpp` proves the same
already-expired stop for seven engines in-process and for the standalone
SQP path's own absolute deadline. This is a measured bound for the tested
envelope on one host, not a formal worst case for every input; the
sections named in §4 remain non-preemptible and are bounded by their
input, iteration, pass and fill caps, and user callback bodies run to
completion by design.

## 4c. Measured peak-memory envelope (IR-21, 2026-10-03)

Budget behavior is measured through the production CLI by
`scripts/peak_memory_envelope.py` and recorded in
`evidence/peak-memory-envelope-2026-10-03.json` (0 failures, 66 s, three
sections).

- **Budget ladder** — per instance a `--memory-limit-bytes` ladder from 1 byte
  to 2× the unlimited charged peak: `e226` (LP, peak 157,280 B), `QPLIB_0010`
  (QP, 6,840 B) and `stein15` (MILP, 97,000 B). Every 1-byte run refused the
  first charge with `ResourceLimit` + `memory_budget_exhausted`, a
  memory-naming message and `failure_site = "memory_budget"`; admitted bytes
  never exceeded the configured limit; the charged peak was monotone in the
  limit; a mid-ladder limit admitted the first charge and then refused a
  later one naming a factor workspace (`sparse basis factor exceeds the solve
  memory budget`, `sparse QP KKT factor exceeds the solve memory budget`,
  root-relaxation factor refusal for the MILP); every 2×-peak run solved
  optimally with a published positive `peak_rss_bytes`.
- **Adversarial factor fill** — a deterministic dense 150×150 LP charged
  885,176 B (RSS 11,456,512 B) against 43,160 B (RSS 5,677,056 B) for its
  banded control; one shared budget of 619,623 B refused the dense instance
  at its sparse basis factor while the banded control solved `Optimal` under
  the same limit.
- **IR-19 transient-RSS follow-up** — 16 bounded re-measure runs of
  pk1/sp150x300d/gen-ip002 (the historical ~990 MiB instances) published
  `peak_rss_bytes` on every run with none at or above 500 MiB (max
  20,475,904 B); the transient was not reproduced. This section is
  report-only: it is not proof of absence.

These are measured bounds for the tested envelope on one host. Admission is
cooperative and in-process, `peak_rss_bytes` is a publication and never a
ceiling, and the uncharged classes named in §4 stay bounded by their own
fill/dimension caps.

## 5. Boundary tests

| Test | Property |
|---|---|
| `tests/stop_reason_test.cpp` — input byte cap | parser cap → `resource_limit` + `input_limit` + `input_resource_limit` |
| `tests/stop_reason_test.cpp` — LP dimension envelope | named pre-dispatch check throws `std::length_error` → `resource_limit` + `work_limit` |
| `tests/stop_reason_test.cpp` — allocation sweep | mapped host OOM → `resource_limit` + `allocation_failure`; nothing escapes the API |
| `tests/stop_reason_test.cpp` — expired deadline / memory budget | cooperative stops carry their exact reason |
| `tests/stop_reason_test.cpp` — MILP node cap | serial node quota → `quota_exhausted` |
| `tests/stop_reason_test.cpp` — serial MILP time limit | engine-local time limit → `deadline_exceeded` |
| `tests/stop_reason_test.cpp` — serial queue capacity | unattributable engine-internal stop → non-empty (`unspecified_resource_limit`) |
| `tests/stop_reason_test.cpp` — shared cancellation | serial search polls the shared context → `resource_limit` naming `cancelled`, never a proven search |
| `tests/sparse_fill_limit_test.cpp` — factor fill cap | over-full factor throws `length_error` before any partial factor (→ `work_limit` at the boundary); the fill cap remains the pre-allocation preflight |
| `tests/readiness_edge_cases_test.cpp` — factor charge lifecycle | basis-factor fill admits while the factor lives and releases with it; a refused charge throws `length_error` naming the budget and admits nothing; session-cached rebind moves bytes without double-charging; the QP KKT charge releases at solve end and a refusal returns `unsupported` with the budget message |
| `tests/stop_reason_test.cpp` — serial queue charge | refused node charge → `resource_limit` + `memory_budget_exhausted`; honest incumbent/bound only |
| `tests/stop_reason_test.cpp` — peak bounds | normal solves report a non-zero `memory_charged_peak_bytes`; admitted charges never exceed the budget |
| `tests/device_budget_test.cpp` — device budget meter | over-limit charge refuses before the allocator runs; live release keeps per-iteration churn inside the limit; refusal stays sticky for the solve |
| `tests/stop_reason_test.cpp` — device budget mapping | refusal maps to `resource_limit` + `device_memory_budget_exhausted` + `device_memory_budget` |
| `tests/device_budget_test.cpp` — solve behavior | GPU host with a 1-byte device limit → `resource_limit` naming the device budget; host without a device → honest CPU fallback with an empty `stop_reason` |
| `tests/api_test.cpp` | normal solves keep an empty `stop_reason`; deadline/memory paths keep their reason; results publish a positive `peak_rss_bytes` on Linux/macOS |
| `tests/readiness_edge_cases_test.cpp` — expired deadline across engines | already-expired solve-wide deadline → `resource_limit` + `deadline_exceeded` + unverified for primal/dual/ipm/pdlp/milp/parallel/qp; undeadlined control stays off the resource path; expired SQP deadline stops with `resource_limit` |

## 6. Change procedure

Any change to this contract (a new reason, a changed mapping, a changed
fallback string) must update the tables above in the same change, extend
`tests/stop_reason_test.cpp`, and re-run `ctest -j8`. Reported strings are
public output (CLI/Python) and may only be added, never renamed.
