---
type: codebase-decision
tags: [codebase, decision, numerics]
status: verified
verified_on: 2026-09-25
evidence:
  - "docs/decisions/ADR-M0-03-numical-policy.md:3"
  - "docs/architecture.md:57-58"
  - "docs/history.md:48-52"
---

# Centralized Numerical Policy

> Every tolerance, scale and threshold belongs to one versioned policy object; stopping criteria and verification use separate values.

## Observed Facts
- `docs/decisions/ADR-M0-03-numical-policy.md:3`: "Centralize all scales and thresholds in a versioned validated policy; separate stopping and verification; report configured and achieved values. Defaults remain provisional." Status: *proposed for independent review*.
- Two-gate pattern in practice: simplex solving tolerances are distinct from the scale-aware dual-tolerance check introduced by the false-optimal remediation (`docs/history.md:47-52`), and PDLP termination has explicit configured KKT tolerances `1e-4/1e-6/1e-8` (`CHANGELOG.md:31`).
- Reporting both configured and achieved values appears in evidence columns: `max_primal_violation`, `max_dual_violation`, `relative_error` (`evidence/netlib_results.csv:1` header; `evidence/miplib_results.csv:1` adds `max_integrality_violation`).
- Scaling safeguards are bounded: Ruiz ℓ∞ equilibration keeps entries in `[1e-4, 1e4]` (`docs/architecture.md:56-57`).
- Verification tolerances are re-checked independently: "independent zero-trust KKT certificate verifier (residuals and complementarity)" (`CHANGELOG.md:14`).

## Impact (Inference)
- Separating *stopping* from *verification* tolerances is the mechanism behind [[Numerical Error]] detection in the audit regressions; a single shared epsilon would have hidden the false-optimal defect.
- "Defaults remain provisional" means published objective agreements (e.g. rel err ≤7.9e-15 in `evidence/netlib_results.csv`) are tied to an unstated policy version.

## Related
- [[status-certificate-fail-closed]] · [[clean-room-provenance]] · [[csc-sparse-storage]]
