# Provenance and verification

## Provenance

The M0 implementation baseline is dated 2026-09-13. Historical records state that
no external solver implementation source was consulted during that coding period,
but this has not been independently checked against access history or a source
trace. The competitive-landscape report later records that 27 peer solver
repositories were cloned and inspected for line-level verification on 2026-09-25
(`docs/audit/19-competitive-landscape.md`, §§19.0, 19.9). That is source-level
exposure during the project, even though the stated purpose was retrospective
competitive analysis. The effect, if any, on implementation has not been
independently determined.

Therefore clean-room status is **unverified** and must not be claimed without
qualification. Production external-solver linkage remains prohibited and absent;
this dependency fact does not prove source independence. A release claim requires
an independent review of exposure dates, relevant history/diffs and algorithm-level
similarities, with quarantine and reimplementation where the review requires it.

**AI-assisted implementation:** Substantial portions of the codebase were produced
with AI code-generation assistance under human direction and review. The authors
retain responsibility for design decisions, mathematical correctness checks,
and verification of every claim that appears in documentation or benchmarks.
This disclosure is intentional; concealment would be more damaging than use.

Each later change should record sources consulted, source exposure, derivation
references, affected invariants, and reviewer status. If review identifies an
affected component, quarantine it and reimplement from approved requirements and
independently derived mathematics.

## Release verification

Run `./scripts/verify-release.sh` from the project root. A passing report
establishes only that required files are intact and the governance/build/schema
checks pass in the local environment. It does **not** establish solver
correctness or performance.

Any source change that affects the packaged release requires regenerating the
source manifest, committing, and repackaging.
