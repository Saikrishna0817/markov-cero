# Hosted service hard-limit contract (v1.1)

**Scope:** the optional HTTP adapter in `web/backend/server.py` and any
deployment built from `web/backend/Dockerfile`. The library contract
([resource-limits.md](resource-limits.md)) stays cooperative by design; this
page is the separate, kernel-enforced contract the blueprint requires for the
hosted service: *spawn each solve with OS CPU/memory/file/output/wall limits
and kill-on-timeout*. v1.1 adds the request quota, the metrics surface and
the result-field passthrough the release task requires; every v1 limit row is
unchanged.

**Model:** one child process per `POST /solve`. The adapter never threads a
solve into the HTTP process. The child is started through a thin wrapper that
applies `setrlimit` and then `execvp`s the solver, so the wall-clock kill
lands on the solver itself (same PID) and a threaded server never forks with
`preexec` hooks.

## 1. Declared limits

All values are environment-overridable and read at spawn time.

| Limit | Declared default | Env knob | Enforcement | Failure surfaced as |
|---|---|---|---|---|
| Wall clock | 15 s | `SOLVE_WALL_TIMEOUT_SECONDS` | `subprocess` timeout, child killed | HTTP 504 |
| CPU time | 12 s soft, 15 s hard | `SOLVE_RLIMIT_CPU_SECONDS` | `RLIMIT_CPU` → `SIGXCPU`/`SIGKILL` | HTTP 422, `terminated by signal N` |
| Address space | 1 GiB | `SOLVE_RLIMIT_AS_BYTES` | `RLIMIT_AS` | HTTP 422 (no result / signal) |
| File size | 16 MiB | `SOLVE_RLIMIT_FSIZE_BYTES` | `RLIMIT_FSIZE` on stdout/stderr/result files | HTTP 422 (no result / signal) |
| Result payload | 2 MiB | — | size check before responding | HTTP 413 |
| Detail read-back | 4096 bytes | — | `read_capped` | truncated detail only |
| Request body / model | 1.1 MB / 1 MB | — | header and text checks | HTTP 413 / 400 |
| Concurrent solves | 2 | — | bounded semaphore | HTTP 429 |
| Solve requests per client window | 60 per 60 s | `REQUEST_QUOTA_MAX_REQUESTS`, `REQUEST_QUOTA_WINDOW_SECONDS` | fixed-window counter keyed by client address, checked before authentication | HTTP 429 + `Retry-After` |

Inside the child the cooperative CLI caps still apply (`--time-limit 10`,
`--max-input-bytes 1000000`, `--max-nodes 1000`, `--max-queued-nodes 1000`).
A cooperative stop returns a `ResourceLimit` JSON result through the normal
200 path; the OS limits above only bind when the child overruns them.

## 2. What the contract claims

- A solve child cannot run longer than the wall timeout: the adapter kills it
  and answers 504.
- A solve child cannot burn more than the declared CPU seconds, cannot map
  more than the declared address space, and cannot grow any single output
  file beyond the declared file-size cap — the kernel enforces these even if
  the solver never polls its shared context.
- Output read back into a response is capped (`read_capped`, result size
  check), so a verbose child cannot inflate a response without a bound.
- `web/backend/os_limits_test.py` (CTest `hosted_os_limits`) drives every
  row above with small caps and proves both that each limit binds and that a
  real solve still returns `Optimal` under the declared defaults.

## 3. What this contract does not claim

- **`RLIMIT_AS` is address space, not RSS.** The same honesty rule as the
  library contract applies: a declared byte cap is not a resident-set
  ceiling.
- **`RLIMIT_CPU` counts CPU seconds, not wall time**; a mostly-sleeping child
  can still reach the wall timeout first.
- **The caps cover the solver child only.** The Python HTTP process, the
  static frontend and any reverse proxy are outside this contract; container
  or platform limits (Docker `--memory`, cgroups) are complementary and
  remain the deployer's responsibility.
- **Authentication and origin checks are not resource limits** and are
  documented in `web/README.md`, not here.
- **The request quota is per client address, not per identity.** Several
  users behind one NAT share one budget, and a spoofed source address cannot
  be distinguished at this layer. It is a load-shedding control, not a
  billing or fairness mechanism.
- **Metrics are counters, not telemetry.** No retention, no cross-process
  aggregation and no availability guarantee attaches to `GET /metrics`, and
  its absence never fails a solve.

## 4. Request quota, metrics and result fields (v1.1)

**Request quota.** `POST /solve` counts requests per client address in a
fixed window of `REQUEST_QUOTA_WINDOW_SECONDS` (default 60) seconds. The
`REQUEST_QUOTA_MAX_REQUESTS`-th request in a window (default 60) is refused
with HTTP 429 and a `Retry-After` header naming the seconds left in the
window; the counter then expires and the window restarts. The check runs
before authentication, so an unauthenticated flood is charged to the same
budget as an authenticated one, and it is applied only to `/solve` —
`/health` and `/metrics` stay cheap for probes.

**Metrics.** `GET /metrics` returns a single bounded JSON document. Gauges:
`solver_ready`, `slots_in_use`, `slots_total`. Counters: `requests_total`
(every response the adapter sends), `solves_total` (every `POST /solve`
received, before any check), `solves_accepted` (`200` on `/solve`),
`solves_by_status` (per solver status string), `quota_rejected` (429 from
the window), `busy_rejected` (429 from the two solve slots), `auth_rejected`
(401), `client_error` (other 4xx — the quota, slot, `422` and `504` outcomes
are excluded and counted under their own names), `server_error` (other 5xx),
`os_limit_kills` (`422` where the kernel signalled the child),
`solver_failures` (`422` with no result, and `502` invalid JSON) and
`timeouts` (504). The document is built before the
response carrying it is counted, so its own `requests_total` does not include
itself. Counters are process-lifetime monotonic; the response
carries no model text, no objective, no filename and no client identity, so
the endpoint is readable without authentication. It is not a monitoring
system: there is no history, no aggregation across processes and no
guarantee of scrape continuity.

**Result fields.** The adapter forwards a fixed whitelist of solver JSON
fields. `assurance`, `lp_iterations`, `relative_gap` and `stop_reason` are
part of the whitelist as of v1.1, so the typed `assurance` label reaches
HTTP consumers exactly as it reaches CLI and Python consumers
([numerical-policy.md](numerical-policy.md)). Fields absent from a given
result are omitted, never invented; the whitelist may only gain fields, never
rename or remove them.

## 5. Change procedure

Changing any default in `web/backend/server.py` must update the table above,
extend the tests so the changed limit still binds (`web/backend/os_limits_test.py`
for the OS limits, `web/backend/service_limits_test.py` for the quota,
metrics and field whitelist), and re-run `ctest -j8`. Reported HTTP statuses
and the `terminated by signal N` detail are public output and may only be
added, never renamed.
