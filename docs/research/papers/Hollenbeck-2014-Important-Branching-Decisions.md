---
type: paper
title: "How Important Are Branching Decisions: Fooling MIP Solvers"
authors: "Hollenbeck, Antonsen, Ralphs et al."
year: 2014
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ○
status: standard
tags: [paper, branching]
---

# How Important Are Branching Decisions: Fooling MIP Solvers

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Shows branching rules can be gamed: adversarial instances make supposedly strong rules behave arbitrarily badly — robustness evidence for default choices.

## Metadata
| Field | Value |
|---|---|
| Authors | Hollenbeck, Antonsen, Ralphs et al. |
| Year | 2014 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified; PDF link in list) |

## Problem Addressed
Branching-rule comparisons are usually favorable to the rule being proposed. The paper asks how much branching decisions really matter and demonstrates instances engineered to fool particular rules.

## Core Contribution
- **Methodology:** Construct instances/behaviors on which standard rules (most-infeasible, pseudo-cost, strong) make provably poor choices; quantify the spread of outcomes across rules.
- **Assumptions:** Instrumented solver; control over instance structure.
- **Benchmarks/datasets:** Designed instances plus standard sets.
- **Metrics:** Node count spread across rules; worst-case behavior per rule.
- **Key results:** Branching matters enormously (orders of magnitude), and no rule is safe everywhere — justifying hybrid/reliability designs (#127/#131) and multi-seed benchmarking.

## Engineering-Relevant Knowledge
**Algorithms:** Failure modes of most-infeasible, pseudo-cost and strong branching.
**Techniques:** Adversarial instance construction as a testing method for our own rule.
**Implementation details:** Suggests a regression suite: instances chosen to stress `src/milp/branch_selector.cpp` and verify the fallback to strong branching triggers when pseudo-costs mislead.
**Equations/rules:** none (empirical/adversarial study).
**Limitations/failure cases:** Designed worst cases may not appear in industrial sets; conclusions are about robustness, not average performance.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Justifies reporting *variance* across instances (and seeds) rather than a single node count — feeds directly into honest R16 comparison methodology.

## Evidence → Engineering Decision
- *Finding:* rule performance varies wildly by instance → *PS requirement:* R16, R20 → *Component:* src/milp/branch_selector.cpp → *Metric:* node-count spread, [[Geometric Mean Runtime]]

## Related Papers
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Linderoth-2000-Impact-Branch-Bound]]
- [[Berthold-2006-Hybrid-Branching]]
- [[Su-2025-Investigating-Exact-Effectiveness]]

## Uses
- [[Strong Branching]] [[Pseudo-Cost Branching]] [[Branch and Bound]] [[Geometric Mean Runtime]]
