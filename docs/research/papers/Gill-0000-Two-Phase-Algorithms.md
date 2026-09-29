---
type: paper
title: "Two-Phase Algorithms / Farkas certificate extraction"
authors: "Gill, Murray, Saunders & Wright; Fourer, 1983; Andersen & Andersen"
year: 0000
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ★
status: deep
tags: [paper, numerics]
---

# Two-Phase Algorithms / Farkas certificate extraction

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Produces *provable* infeasibility/unboundedness: a Farkas certificate (y ≥ 0, yᵀA ≥ 0, yᵀb < 0) that independently verifies why no solution exists.

## Metadata
| Field | Value |
|---|---|
| Authors | Gill, Murray, Saunders & Wright; Fourer, "Testing a LP for Infeasibility", 1983; Andersen & Andersen |
| Year | **first item year not stated in list** — slug uses 0000 (Fourer 1983 listed); see manifest |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Returning "infeasible" from a floating-point Phase I is a *claim*; on near-degenerate models it is often wrong, and downstream users cannot check it. A verifiable certificate turns a status code into a proof.

## Core Contribution
- **Methodology:** Two-phase simplex with explicit infeasibility detection, and extraction of a Farkas vector (or unbounded ray) from the Phase I basis so an independent checker can verify the claim arithmetically.
- **Assumptions:** Phase I terminates with a recognizable infeasible/unbounded basis; certificate checked in exact or higher precision.
- **Benchmarks/datasets:** Infeasible/unbounded LPs (NETLIB has such cases).
- **Metrics:** Certificate verification success; false-infeasible rate; cost of extraction.
- **Key results:** Certificates are cheap (a byproduct of the basis) and are the difference between "solver says infeasible" and "we can prove infeasible".

## Engineering-Relevant Knowledge
**Algorithms:** Two-phase simplex infeasibility detection; Farkas certificate extraction; unbounded ray extraction.
**Techniques:** Verify certificates in exact arithmetic (ties to #155/#157); store certificates alongside status codes.
**Implementation details:** We have Phase I/II (`src/lp/reference/revised_simplex.cpp`) and status handling (ADR-M0-02 status certificates) — extracting and *checking* the Farkas vector in `src/verify/` would close the loop for R17. Also needed to distinguish "infeasible" from "numerically failed" (our `BLEND → NumericalFailure` bug is exactly this ambiguity).
**Equations/rules:** Infeasibility certificate: y ≥ 0, yᵀ A ≥ 0, yᵀ b < 0 ⇒ Ax = b, x ≥ 0 impossible. Unbounded ray: d ≥ 0, Ad = 0, cᵀd < 0 with feasible x, x + λd feasible ∀λ.
**Limitations/failure cases:** A numerically-infeasible basis can yield a certificate that fails exact verification — then the correct status is "numerical failure", not "infeasible".

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Turns our status codes into proofs (R17, R18 transparency) and fixes the infeasible-vs-failed confusion visible in our GPU benchmark data.

## Evidence → Engineering Decision
- *Finding:* `BLEND → NumericalFailure` without certificate → *PS requirement:* R13, R17, R18 → *Component:* src/lp/dual/dual_simplex.cpp, src/verify/ → *Metric:* certificate verification rate, false-status rate

## Related Papers
- [[Gleixner-2015-Iterative-Refinement-Linear]]
- [[Steinrucken-2019-Exact-Algorithms-Linear]]
- [[Neumaier-2004-Safe-Bounds-Linear]]
- [[Gill-1974-Methods-Modifying-Matrix]]

## Uses
- [[KKT Conditions]] [[Duality Gap]] [[Numerical Stability]] [[Iterative Refinement]]
