---
type: concept
tags: [datasets, benchmark]
status: stable
verified_on: 2026-09-25
---

# QPLIB

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The standard QP instance library — the benchmark R19 names for the quadratic half of the scope.

## Definition
QPLIB is a curated library of convex and nonconvex quadratic programming instances in a standard format, spanning pure QP, QP with linear constraints and mixed-integer variants, drawn from applications where quadratic objectives arise (portfolio, process, design). It exists because QP solvers historically reported on private sets, making cross-solver claims unauditable; a shared library lets accuracy, convexity handling and runtime be compared on identical data. For a solver claiming QP support (R2), QPLIB is the recognized place to demonstrate it — including how it handles nonconvex instances (solve, reject, or report a certificate).

## Why It Matters Here
- R19 lists QPLIB in the Dataset Link text; R2 makes QP part of the initial scope, so a QP claim without QPLIB results is unverifiable.
- Observed state: the repo's QP engine is ADMM over a quasi-definite KKT system with convexity screening that **rejects** non-convex models (src/qp/model.cpp:142-206) and the dimension cap is 4096 (src/qp/model.cpp:11-12) — Inference: a QPLIB sweep would immediately expose which library instances fall inside those gates.
- Observed state: no QPLIB instance results appear in `evidence/` or `reports/` (miplib/netlib/crossover/gpu artifacts only).

## Key Facts / Rules
- Report convex-instance results separately from non-convex handling behavior — they are different claims.
- QP accuracy metric: KKT residual (stationarity + feasibility + complementarity), not objective difference alone.
- Small convex QPs suit active-set/dual methods; large sparse QPs suit first-order/ADMM — the crossover point is empirical.
- Format/tools: standard QPLIB file format and its checker; verify returned solutions with them.

## Related
- [[ADMM]]
- [[KKT Conditions]]
- [[Relative Optimality Gap]]
- QP-ADMM-Engine
- [[Interior-Point Method]]

## Referenced By

- 21-traceability
- Datasets MOC
- Research MOC
- cross-paper-synthesis
- [[Goldfarb-1983-Numerically-Stable-Dual|research/papers/Goldfarb-1983-Numerically-Stable-Dual]]
- [[Goldfarb-1984-Dual-Primal-Dual-Methods|research/papers/Goldfarb-1984-Dual-Primal-Dual-Methods]]
- [[Wright-1997-Primal-Dual-IPM|research/papers/Wright-1997-Primal-Dual-IPM]]