---
type: engineering-decision
id: ED-008
status: accepted
date: 2026-09-25
tags: [engineering-decision, verification, trust, r17, r18, keep]
---

# ED-008 — Retain and Promote the Zero-Trust Verifiers

> The three independent verifiers are the trust layer and stay mandatory: no engine reports a terminal status an independent recomputation has not confirmed, and certificates are emitted in comparison and demo output.

## Context (Observed fact)

- Three tested checkers exist: canonical KKT/Farkas (`src/verify/reference_lp_verifier.cpp:53`), original-space primal (`src/verify/primal_verifier.cpp:47`), QP KKT certificate (`src/qp/verifier.cpp:46`); the CLI computes `verified = optimal ∧ original ∧ canonical` (`apps/markov_cero_solve.cpp:439-443`).
- 09 §6.5 grades independent dual-gated verification as **stronger than the research baseline** and a differentiation to keep.
- Two known holes are recorded in `IndependentVerifiers`: PDLP results get only a primal check, and the MILP/parallel branches hardcode `canonical_verified = true` without an explicit canonical re-check.

## Research Evidence

- [[Unknown-2026-Verified-Linear-Programming]] — verification of LP results as a first-class deliverable, not an afterthought.
- [[KKT Residual]] — the metric that makes "verified" quantitative and reportable per instance.
- [[Numerical Error]] — allowance accounting (`tol·scale + 512ε·max(1,scale)`) already implemented in the canonical verifier.
- [[status-certificate-fail-closed]] — repo decision: failure to verify downgrades status.
- [[IndependentVerifiers]] — component note with the exact file:line evidence.

## Decision

- Keep all three verifiers; they are never simplified for benchmark convenience.
- Close the two recorded holes: run a canonical/dual check on the PDLP path (or mark its results explicitly `primal-only`), and replace the hardcoded `canonical_verified = true` in the MILP/parallel branches with a real check.
- Every new engine — including the IPM + crossover from [[ED-003-interior-point-required-by-ps]] — must pass the same dual gate before it can report `optimal`.
- Emit certificate fields (`verified`, max violations, KKT residuals) as columns in the ED-001 comparison tables and in demo output, so the comparison is quality-safe and not merely fast.

## Consequences (positive / negative / neutral)

- **positive:** a "certified sovereign solver" narrative with machine-checkable evidence; turns the verification layer into a demo asset; protects R17/R18.
- **negative:** some runs downgrade to `numerical_failure`, so headline success counts may fall; small CPU overhead per solve.
- **neutral:** verifiers already exist — this is wiring and hole-closing, not new research.

## Alternatives Rejected

- *Trust solver status strings* — rejected by the corpus: status is not evidence (cross-paper §1).
- *One verifier instead of three* — loses the original-space gate that catches canonicalization bugs.
- *Drop verification for benchmark speed* — destroys the project's only structural advantage over established solvers (09 §6.5).

## Linked Requirements

- R9, R13, R17, R18 → [[sih26119_problem_statement]]

## Related

- [[09-research-code-alignment]] (§6.5) · [[12-keep-remove-rebuild]] (§8.1 KEEP) · [[13-restart-point]] (step 5) · [[21-traceability]] §21.4 row 10
- [[ED-001-comparison-harness-before-new-algorithms]] · [[ED-003-interior-point-required-by-ps]]
