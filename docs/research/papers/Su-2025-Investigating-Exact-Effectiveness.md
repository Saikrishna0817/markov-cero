---
type: paper
title: "Investigating the Exact Effectiveness of Cutting Planes over Branch-and-Bound in Integer Programming"
authors: "Su"
year: 2025
venue: "(not stated in list; JHU repository link)"
doi: "(unverified)"
domain: [cuts, branching]
priority: ✦
status: standard
tags: [paper, cuts]
---

# Investigating the Exact Effectiveness of Cutting Planes over Branch-and-Bound in Integer Programming

> Shows, for 3D convex 0/1 IPs, that branch-and-cut proof trees can be reduced to pure cutting-plane trees — a structural case for cuts over branching.

## Metadata
| Field | Value |
|---|---|
| Authors | Su |
| Year | 2025 |
| Venue | (not stated in list; Johns Hopkins repository item) |
| DOI/URL | (unverified; JHU link in reference list) |

## Problem Addressed
Whether cutting planes or branching is the "real" driver of MILP progress is usually settled empirically. This work asks it exactly (proof-theoretically) on a restricted but nontrivial class.

## Core Contribution
- **Methodology:** Formal reduction argument: for three-dimensional convex 0/1 integer programs, every branch-and-bound/branch-and-cut proof tree can be transformed into an equivalent pure cutting-plane (separation-based) tree.
- **Assumptions:** 3D convex 0/1 IP class; exact arithmetic for proofs.
- **Benchmarks/datasets:** None — proof-theoretic result.
- **Metrics:** Existence of a polynomial/size-bounded cut tree; proof-tree size.
- **Key results:** On that class, cuts subsume branching — a theoretical counterweight to the common engineering choice of "few cuts, lots of branching".

## Engineering-Relevant Knowledge
**Algorithms:** Proof-tree transformation; separation-based solving vs. tree search.
**Techniques:** Supports investing in separation quality (root + tree) over purely mechanical branching improvements.
**Implementation details:** Not directly implementable; relevant because our solver is the opposite pole (root-only cuts, full tree search) and the audit shows that costs us 0.0% node reduction.
**Equations/rules:** none (proof result).
**Limitations/failure cases:** Restricted to 3D convex 0/1; says nothing about runtime of the constructed cut tree — existence ≠ efficiency.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Useful rhetorical/theoretical support for prioritizing cut separation (R5) but offers no algorithm we can run; do not cite it as evidence of runtime benefit.

## Evidence → Engineering Decision
- *Finding:* cuts can replace branching on a provable class → *PS requirement:* R5 → *Component:* src/milp/milp_solver.cpp → *Metric:* node count, [[Cut Efficiency]]

## Related Papers
- [[Padberg-2005-Classical-Cuts-Mixed]]
- [[Balas-1996-Gomory-Cuts-Revisited]]
- [[Hollenbeck-2014-Important-Branching-Decisions]]
- [[Linderoth-2000-Impact-Branch-Bound]]

## Uses
- [[Branch and Bound]] [[Branch and Cut]] [[Cut Validity]] [[LP Relaxation]]
