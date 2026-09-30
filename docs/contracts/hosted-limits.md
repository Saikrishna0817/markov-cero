# Hosted service hard-limit contract (v1)

**Scope:** the optional HTTP adapter in `web/backend/server.py` and any
deployment built from `web/backend/Dockerfile`. The library contract
([resource-limits.md](resource-limits.md)) stays cooperative by design; this
page is the separate, kernel-enforced contract the blueprint requires for the
hosted service: *spawn each solve with OS CPU/memory/file/output/wall limits
and kill-on-timeout*.

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

## 4. Change procedure

Changing any default in `web/backend/server.py` must update the table above,
extend `web/backend/os_limits_test.py` so the changed limit still binds, and
re-run `ctest -j8`. Reported HTTP statuses and the `terminated by signal N`
detail are public output and may only be added, never renamed.
