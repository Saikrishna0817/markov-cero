---
type: paper
title: "CBC Solver Documentation (black box vs. framework)"
authors: "COIN-OR"
year: "n.d."
venue: "COIN-OR documentation"
doi: "(unverified)"
domain: [milp]
priority: ✦
status: standard
tags: [paper, milp]
---
# CBC Solver Documentation (black box vs. framework)
> Living documentation of CBC's API: black-box solver vs. embeddable framework, and what that means for interface design.
## Metadata
| Field | Value |
|---|---|
| Authors | COIN-OR |
| Year | n.d. (documentation, continuously updated) |
| Venue | COIN-OR documentation |
| DOI/URL | https://www.coin-or.org/Cbc/ |
## Problem Addressed
Users need CBC two ways: as a command-line/library black box that solves a model, and as a framework whose cut generation, branching and heuristics are replaceable. The docs articulate that split and the APIs behind it.
## Core Contribution
- **Methodology:** Documents CbcModel as the driver object, Cgl cut generators, branching objects/heuristics as separately constructible components; describes parameter and callback surfaces.
- **Assumptions:** C++ library consumers; solver used both ways.
- **Benchmarks/datasets:** None (documentation).
- **Metrics:** Interface coverage; usability.
- **Key results:** Practical API reference showing how far componentization can go while keeping a simple entry point (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-cut exposed as configurable objects.
**Techniques:** Model object owning search state; generators passed in explicitly.
**Implementation details:** Our CLI (`--engine`, `--branching`, `--cuts`, `--presolve`) is the "black box" surface; a documented C++ API layer (R14: API or CLI sufficient) is the framework surface — both already exist in embryonic form.
**Limitations/failure cases:** Documentation drifts from code; treat as design reference, not ground truth (and not buildable-from per R10).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R14 (API/CLI), R1 (core), R18 (sovereign, transparent) — comparative reference for how a solver exposes configuration without leaking internals.
## Evidence → Engineering Decision
- *Finding:* Solvers succeed by offering both black-box and framework surfaces → *PS requirement:* R14, R1 → *Component:* apps/markov_cero_solve.cpp + include/markov_cero/ public headers → *Metric:* CLI/API feature parity checklist.
## Related Papers
- [[Linderoth-2005-Noncommercial-Software-Mixed]]
- [[Bixby-2020-Compiling-Mixed-Integer]]
## Uses
- [[Branch and Cut]]
- [[Warm Start]]
