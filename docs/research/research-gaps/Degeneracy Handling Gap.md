---
type: research-gap
tags: [research-gaps, numerics]
status: stable
verified_on: 2026-09-25
---

# Degeneracy Handling Gap

> Degeneracy is named in the problem statement and answered with a safety rule — the anti-cycling floor exists, the anti-degeneracy toolbox does not.

## Definition
The gap between "the simplex terminates on degenerate LPs" and "the simplex *performs* on degenerate LPs": present defenses are Bland tie-breaking and a Harris ratio test, while the standard production set additionally includes steepest-edge pricing, expanding tolerances (EXPAND) or perturbation to break ties without destroying pivot quality, dual-phase bound-flipping, and detection/reporting of degeneracy itself. There is also a demonstration gap: test suites exercise degenerate cases, but no curated report shows a hard degenerate instance solved reliably with metrics — which is what R17 asks for.

## Why It Matters Here
- R13 lists "highly degenerate models" as a benchmark requirement and R17 requires a *clear demonstration* of robustness on them.
- Observed state: pricing is Bland-only, `tableau_norm` is explicitly not dual steepest-edge, no EXPAND/perturbation exists (src/lp/reference/revised_simplex.cpp:128-173; include/markov_cero/lp/dual/dual_simplex.hpp:12-14).
- Inference: degeneracy *tests* passing ≠ degeneracy *performance* demonstrated — the audit separates these (C.3 "no curated hard-instance demonstration report").

## Key Facts / Rules
- Defense layers: (1) finite termination [have], (2) tolerance hygiene [partial — Harris in dual only], (3) pivot quality [missing — steepest edge], (4) tie-breaking beyond Bland [missing — perturbation/EXPAND], (5) measurement [missing — iteration counts on known-degenerate sets].
- Degeneracy metrics: iterations to optimality on degenerate Netlib/MIPLIB members vs a baseline, stall frequency, pivots with zero step.
- Detection: count zero-step pivots and duplicate bases — cheap telemetry that turns the gap into data.
- Note: cloud branching (Berthold 2013) exploits alternate optima *productively* — degeneracy is not only a hazard.

## Related
- [[Degeneracy]]
- [[Bland Anti-Cycling]]
- [[Steepest Edge]]
- [[Harris Ratio Test]]
- [[Ill-Conditioned Instance Dossier]]
- [[Maros-2003-Generalized-Dual-Phase]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Research-Code Traceability MOC|research/Research-Code Traceability MOC]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]