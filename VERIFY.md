# Release verification

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

- **Correctness on benchmarks**: `ctest` includes the Netlib/MIPLIB regression targets; the
  cross-solver comparison vs HiGHS (R16) is a separate artifact produced by
  `scripts/run_compare.py` into `evidence/benchmarks/compare/`.
- **Performance claims**: every performance number quoted in docs is bound to the machine
  manifest in `evidence/hardware.md` and a reproducible runner script under `scripts/` or
  `benchmarks/runners/`.

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

Default API proof generation/replay has a five-second budget inside the solve
budget, 10,000 nodes and four million witness values. Failure to finish leaves
`canonical_verified=false`; a feasible incumbent is still separately identified.
`verified` requires the relevant global numerical proof, not CLI exit code alone.
Gap-satisfied proofs need an explicit relative-gap allowance in the C++ checker;
the standalone CLI intentionally uses the strict default gap.

Current qualification results and source/executable hashes are in
[evidence/readiness-checkpoint.json](evidence/readiness-checkpoint.json).
The [closure register](evidence/defect-closure-register.csv) lists every baseline
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
[evidence](evidence/readiness-validation/production-proof-harness.txt).
