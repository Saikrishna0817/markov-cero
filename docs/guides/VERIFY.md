# Release verification

This guide describes checks a contributor can rerun from the repository root.
Historical test counts in the [changelog](../../CHANGELOG.md) and
[evidence index](../../evidence/INDEX.md) belong to their recorded builds.
After source or directory changes, rerun the relevant checks and record the new
binary or source revision before presenting a result as current.

## What `scripts/verify-release.sh` does

Run from the repository root:

```bash
./scripts/verify-release.sh
```

The script performs, in order:

1. **Environment capture** — writes `evidence/environment-local.json` (OS, machine, Python version).
2. **Sovereignty guard** — runs `scripts/check-sovereignty.py` over the tree (R10: no third-party
   optimization-library linkage or imports).
3. **Dual-compiler build + test** — configures and builds the full CMake project twice
   (GCC and Clang, Release), then runs the CTest suite for each, under a per-run timeout when
   `timeout` is available.

All step output is appended to `evidence/local-verification-report.txt`.

## What a passing report establishes — and what it does not

A passing report establishes only that, **in the local environment**: the sovereignty guard
finds no external-solver contamination, the tree compiles warning-clean under two independent
compilers, and the test suite passes.

It does **not** establish solver correctness on unseen models, numerical robustness on hostile
instances, or any performance claim. For those:

- **Correctness on benchmarks**: the default CTest suite contains small solver
  regressions. Optional full Netlib/MIPLIB integration tests require
  `MARKOV_CERO_ENABLE_BENCHMARK_TESTS`; the separate HiGHS comparison runner is
  `scripts/run_compare.py`, which writes to `evidence/compare/` by default.
- **Performance claims**: every performance number quoted in docs is bound to the machine
  manifest in `evidence/hardware.md` and a reproducible runner under `scripts/`.

## Regenerating evidence after source changes

The script writes into `evidence/`, which is tracked. When a source change alters any
performance-relevant behavior, re-run the affected runner (not just this script) and commit the
regenerated artifact together with the change.

## Numerical MIP proof replay

CLI/Python results expose `mip_proof` and `proof_message`. Save the proof string
verbatim, then run `markov-cero-verify-mip MODEL.mps PROOF.txt`. Exit 0 means
accepted; nonzero means rejected, unsupported, incomplete or malformed. The C++
checker also accepts explicit tolerance, relative-gap and budget options.

The generator makes a separate cut-free tree over the original integer domain.
The checker performs no optimization: it validates the incumbent, every child
partition, every leaf certificate, reachability, and the final bound. Linear
MILP uses LP/Farkas witnesses; convex MIQP uses PSD/KKT/supporting-bound or Farkas
witnesses. It shares canonicalization and numerical verifier primitives with the
core, so it is independent of the search/cut logic, not a second implementation
of input parsing or exact arithmetic. It does not certify OA/MINLP or prove that
a refinery formulation represents a physical plant.

API/CLI callers can configure independent proof time/node/witness budgets. The
defaults are five seconds, 10,000 nodes and four million witness values, within
the API/CLI caps of 100,000 nodes and 16,000,000 witness values. CLI flags are `--proof-time-limit SEC`,
`--proof-max-nodes N` and `--proof-max-values N`. Python accepts
`proof_time_limit`, `proof_max_nodes` and `proof_max_witness_values`, or these
values on `SolveOptions`. The standalone checker accepts `--time-limit SEC`,
`--max-nodes N`, `--max-values N` and `--relative-gap T`; its proof time covers
bounded proof parsing and replay. The generator proof deadline is clipped to the
overall solve deadline. Failure to finish leaves `canonical_verified=false`; a feasible
incumbent is still separately identified.
`verified` requires the relevant global numerical proof, not CLI exit code alone.
Gap-satisfied proofs need an explicit relative-gap allowance in the C++ checker;
the standalone CLI intentionally uses the strict default gap. Result objects, CLI
JSON and Python dictionaries report `mip_proof_build_ms` and
`mip_proof_verify_ms` separately. These measure proof construction and replay,
excluding the primary optimization solve; failed stages report elapsed time in
the stage that failed.

Current qualification results and source/executable hashes are in
[evidence/readiness-checkpoint.json](../../evidence/readiness-checkpoint.json).
The [closure register](../../evidence/defect-closure-register.csv) lists every baseline
item, its disposition, acceptance evidence and remaining limits. Historical
short-cap and mixed timing-boundary reports are retained as historical evidence.
New comparison runs use process-wall timing, including Python startup/imports
for oracle processes, one attempt per sample and matched threads. These measure
application latency; they do not establish pure solver-engine speed or vendor parity.

### Production case proof budget

The default five-second proof stage for `production_planning_large` exhausts its
budget and correctly leaves global verification false. A separate C++ API run
with a 45-second generation budget and a 10-second replay budget accepted all
157 nodes at a requested relative gap of 0.05. This certifies the 5% gap, not
zero-gap optimality. The harness and replay result are retained in
[evidence](../../evidence/readiness-validation/production-proof-harness.txt).

## MINLP independent OA proof replay

Convex quadratic MINLP results expose `oa_proof`, `oa_proof_build_ms`,
`oa_proof_verify_ms`, `assurance` and `guarantee_tier`. The proof string
begins with the header `MARKOV_OA_PROOF 1` — format version 1, checked by
strict equality in the writer, the reader and the verifier
([contract](../contracts/minlp-proof-replay.md)). Save it verbatim, then run:

```bash
markov-cero-solve MODEL.mps > result.json        # or the Python binding
python3 -c "import json,sys; open('oa.txt','w').write(json.load(open('result.json'))['oa_proof'])"
markov-cero-verify-minlp MODEL.mps oa.txt
```

Exit 0 means accepted; exit 1 means rejected (including a malformed or
truncated proof and an exhausted replay budget); exit 2 is a usage error. A
rejected proof prints `REJECTED:` and names the failed obligation — the
fingerprint, a cut replay, the convexity pivot, the incumbent, the master
tree, the bound or the gap. The checker takes `--time-limit SEC`,
`--max-nodes N`, `--max-values N` and `--relative-gap T`.

**Budgets are shared with the MIP proof.** Generation and replay read
`enable_mip_proof`, `proof_time_limit`, `proof_max_nodes` and
`proof_max_witness_values` (CLI: `--proof-time-limit`, `--proof-max-nodes`,
`--proof-max-values`; the standalone checker's `--max-nodes` bounds the
replay only). Exhaustion reports `proof_status = "exhausted"` with the
budget kind and never converts to `Optimal`: the status becomes `Feasible`
with `assurance = "original_primal_checked"`.

**Tiers.**

| `guarantee_tier` | Meaning |
|---|---|
| `independent_oa` | the fingerprint matched the source model and every obligation (O1–O12) passed |
| `replayed_oa` | all obligations passed but the fingerprint was stripped, so the proof is not tied to this model — replayable, never canonical |
| `unverified` | exhausted, rejected or not requested |

`assurance = "oa_replayed"` is emitted only for an accepted, canonical
result. An accepted proof reports `certificate_type`
`incumbent_feasibility; independent_oa_gap` (or `…_tree` when the relative
gap is exactly 0); an infeasible claim reports `incumbent_feasibility;
independent_oa_tree` alongside `claims_infeasible = true`.

**Limitation.** Replay is floating-point arithmetic against the same model
data. It is an independent re-derivation with documented tolerances — not a
formal exact or machine-checked proof — and it says nothing about whether
the model represents a physical plant. The [record](../../evidence/minlp02-proof-2026-10-01.json)
measures 4/4 accepted on the frozen four-case stratum.
