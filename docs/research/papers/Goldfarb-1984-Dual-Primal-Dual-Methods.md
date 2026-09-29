---
type: paper
title: "Dual and Primal-Dual Methods for Solving Strictly Convex Quadratic Programs"
authors: "Goldfarb & Idnani; Panton"
year: 1984
venue: "(not listed in source)"
doi: "(unverified)"
domain: [qp]
priority: ★
status: deep
tags: [paper, qp]
---

# Dual and Primal-Dual Methods for Solving Strictly Convex Quadratic Programs

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Implementation companion to Goldfarb-Idnani: algorithm variants plus a ready-to-use subroutine (with Panton's 1985 analysis).

## Metadata
| Field | Value |
|---|---|
| Authors | Goldfarb & Idnani (1984); Panton (1985) |
| Year | 1984 |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |

## Problem Addressed
The 1983 paper proved the method; implementers still needed practical details — initialization, step selection variants, a primal-dual hybrid, and code that could be dropped into a program. Panton's companion paper audited the algorithm's behavior.

## Core Contribution
- **Methodology:** Dual active-set variant plus a primal-dual formulation that avoids some backtracking; packaged as an implementable subroutine; Panton gives usage/accuracy guidance.
- **Assumptions:** Strictly convex Q; linear constraints; tolerance-based violation tests.
- **Benchmarks/datasets:** Convex QP test problems (paper's sets).
- **Metrics:** Iterations, violation norms, accuracy vs. primal methods.
- **Key results:** Practical subroutine-level performance on small/medium QPs (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Dual and primal-dual active-set QP.
**Techniques:** Constraint activation order; Cholesky/QR maintenance (continues 1983).
**Implementation details:** Best reference for *coding* an active-set engine: state layout, initialization, termination tests — directly portable to a proposed `src/qp/active_set/` alongside the existing ADMM path.
**Limitations/failure cases:** Same scaling limits as 1983; primal-dual variant needs care to stay complementary (tolerance interplay with [[Duality Gap]] tolerance).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R2 (QP) + R10 (from scratch): a self-contained algorithm spec with a reference subroutine is exactly what a clean-room implementation needs (read for method, write our own code).

## Evidence → Engineering Decision
- *Finding:* Reference subroutine provides test vectors/behavioral expectations for active-set steps → *PS requirement:* R2, R16 → *Component:* src/qp/ (proposed active-set engine) + tests/qp_test.cpp → *Metric:* agreement with ADMM on objective; [[KKT Residual]].

## Related Papers
- [[Goldfarb-1983-Numerically-Stable-Dual]]
- [[Connell-1999-Dual-Active-Set-Algorithm]]
- [[Lawson-1974-Solving-Least-Squares]]

## Uses
- [[KKT Conditions]]
- [[Duality Gap]]
- [[QPLIB]]
