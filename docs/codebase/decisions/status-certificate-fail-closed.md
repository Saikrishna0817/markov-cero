---
type: codebase-decision
tags: [codebase, decision, verification, certificates]
status: verified
verified_on: 2026-09-25
evidence:
  - "docs/decisions/ADR-M0-02-status-certificates.md:3"
  - "docs/history.md:44-70"
  - "tests/regression_test.cpp"
---

# Status Certificates Fail Closed

> Solver statuses are separated from limit errors, and any witness that cannot be independently re-validated fails closed.

## Observed Facts
- `docs/decisions/ADR-M0-02-status-certificates.md:3`: "Separate certified mathematical conclusions from limits/errors. Fail closed when a witness cannot be independently validated. No permissive contradictory-status tests." Status: *proposed for independent review* (not yet approved).
- Corroborating implementation narrative: `docs/history.md:44-70` records the M-series audit remediation — false-optimal (pivot-tolerance thresholding let reduced costs <1e-11 pass), false-infeasible (near-zero Farkas rays accepted), and non-transactional sparse basis updates — each "captured in permanent audit regression tests (`tests/regression_test.cpp`) and verified on every CI run" (`docs/history.md:77`).
- Companion ADR: `docs/decisions/ADR-M0-01-canonical-forms.md:3` requires transformations to "preserve model meaning and certificate signs".
- `audit_regressions` is a standing CTest target (`CMakeLists.txt:178`) and passed in the recorded run (`evidence/local-verification-report.txt:178`).
- QP side applies the same idea: "independent zero-trust KKT certificate verifier" (`CHANGELOG.md:14`), with `qp` test at `CMakeLists.txt:196`.

## Impact (Inference)
- Fail-closed verification is why several tests assert *absence* of false certificates rather than optimality alone — the suite is partly adversarial.
- ADR status "proposed" means the rule is documented but not formally approved; enforcement evidence rests on regression tests, not on the ADR lifecycle.

## Related
- [[clean-room-provenance]] · [[numerical-policy-centralized]] · [[csc-sparse-storage]]
