# markov-cero

markov-cero is a clean-room C++20 research solver for linear and mixed-integer
optimization, with convex quadratic and experimental nonlinear paths. It is a
solver prototype, not a production refinery planning system or a demonstrated
commercial-solver replacement.

## Current release status

The current CPU acceptance run passes **80/80 CTests**; a fresh ML-enabled
Release build also passes 80/80, and the installed Python wheel passes 16/16.
The five previously failing numerical/domain cases pass; production planning
meets its requested 5% MIP gap, while the other four report verified optima.
Maintained code and
build files meet the **300 physical line** limit, enforced by CI.

The [closure register](evidence/defect-closure-register.csv) records **29/32
(90.6%) actionable code defects closed** from the plan's baseline. Four capability
and release gates are listed separately. This is a scoped implementation tally,
not a claim that 90% of all possible bugs are known or fixed. Three engineering
defects remain open: full immutable node views, complete deadline coverage, and
solve-wide memory budgets. The 100% critical/high release gate is therefore unmet.
See [current evidence](evidence/readiness-checkpoint.json).
See [the implementation plan](docs/audit/INDUSTRY-READINESS-IMPLEMENTATION-PLAN.md)
for the remaining acceptance requirements.

## Build and solve

    cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build -j
    ./build/markov-cero-solve examples/blend.mps
    ./build/markov-cero-solve examples/qp_portfolio.mps --engine qp

For the offline qualification example:

    bash run-qualification-demo.sh

Follow [BUILDING.md](BUILDING.md), [QUICKSTART.md](QUICKSTART.md) and
[VERIFY.md](VERIFY.md) for build options and verification commands. Optional
Python, comparison and CUDA checks require their documented dependencies and
must not be inferred from a CPU-only build.

## What the repository demonstrates

- Clean-room C++20 solver implementation and provenance records.
- Sparse model representation, presolve, scaling and multiple LP engines.
- MILP branch-and-cut and a convex QP/MIQP path.
- Independent LP/QP/NLP checks and bounded MILP/MIQP tree replay; see STATUS.
- CLI, C++ API, Python bindings and benchmark/evidence tooling.

These are implementation areas, not blanket claims that every engine or model
class is production-ready. The refinery examples include synthetic models and the public historical Fawley
qualification model; they are not approved MRPL operating data. The GPU evidence currently shows no
end-to-end advantage over CPU PDLP on the measured RTX 2050 cases. ML branching
has not passed its deployment acceptance gate.

## Evidence and project documents

- [Current capability and evidence register](STATUS.md)
- [Current codebase audit](docs/audit/CODEBASE-AUDIT-2026-09-28.md)
- [Consolidated findings](docs/audit/sih_2026_findings.md)
- [Full SIH and engineering audit](docs/audit/sih_2026_implementation_audit.md)
- [Official SIH problem statement](docs/sih26119_problem_statement.md)
- [Provenance and verification](docs/governance/provenance-and-verification.md)
- [Change history](CHANGELOG.md)

The optional dataset migration removes 227 large benchmark files from the current
checkout: retained data is approximately 4 MB instead of 588 MB. Small default
fixtures stay offline. Git history is unchanged; use the
[hash-pinned manifest](data/optional-datasets.json) and explicit restoration:

    python3 scripts/datasets.py --list
    python3 scripts/datasets.py --name gen-ip002
    python3 scripts/datasets.py --family netlib --download

The first restore uses a preserved local cache or the pinned Git revision;
`--download` permits fetching that revision when absent. Missing datasets are
reported as `DatasetUnavailable`, and full runners retain them in the denominator.

## Independent MIP proofs and conflicts

Linear MILP and convex MIQP results can include a cut-free proof tree in JSON
`mip_proof`. The standalone checker replays every integer partition and LP/QP
leaf witness without running a solver:

    ./build/markov-cero-verify-mip model.mps proof.txt
    ./build/markov-cero-iis infeasible.mps

Proofs are numerical, with explicit tolerances and node/witness/deadline budgets.
An incomplete proof remains unverified. OA/MINLP has no global proof export.
Conflict analysis reports row irreducibility relative to unchanged variable
bounds; unknown or timed-out trials cannot establish irreducibility.

## Public refinery qualification data

An attributed [Fawley historical input](data/refinery/fawley_public.json) and
[independently generated MPS](examples/refinery/fawley-public.mps) are available:

    python3 scripts/generators/gen_public_refinery.py
    ./build/markov-cero-solve examples/refinery/fawley-public.mps

The 29-row, 36-column model has explicit process feed consumption, capacities,
recipe blending, and weight/volume quality bases. Markov and the development
HiGHS oracle returned objective −2899.252790423 (thousand historical USD/period).
This is qualification evidence for this generated LP, not validation of a plant.
The source uses approximate blend indices, historical lead cost and energy-equivalent
transfers; see [provenance and limitations](PROVENANCE.md).

Python wheels now build an isolated CPU core with CMake; they do not consume a
pre-existing local build. Python input buffers are copied into owned vectors;
NumPy solution output adopts vector storage. `verified` denotes the API's global
verification result; `original_verified` and `certificate_type` describe narrower
incumbent or local checks.
