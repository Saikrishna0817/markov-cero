---
type: technique
tags: [technique, first-order]
status: stable
verified_on: 2026-09-25
---

# Adaptive Restart

> Restart first-order methods (PDHG/PDHG-style) when momentum/over-smoothing degrades progress.

## Definition
Adaptive restart strategies detect stagnation or divergence in accelerated first-order
methods (e.g. primal-dual hybrid gradient) via duality-gap, gradient-mapping or
function-value conditions, and reset the extrapolation/momentum step to restore convergence
rate guarantees.

## Why It Matters Here
PS R8/R9: the GPU engine is a PDHG solver (`gpu/src/pdhg_step.cpp`) whose README claims an
"adaptive restart strategy", and research (cuPDLP line of work) shows restarts matter for
hard LPs. Observed: repo commits mention "adaptive restart on normalized duality gap"
(T-5.08) but no restart-related test was found — verify before relying on it.

## Key Facts / Rules
- Common trigger: `⟨x_k − x_{k−1}, x̃_k − x_k⟩ > 0`-style O'Donoghue–Candès conditions (Inference: standard form; not verified against this repo).
- Restart interacts with [[Diagonal Preconditioning]] and ergodic averaging.

## Related
- [[Primal-Dual Hybrid Gradient]]
- [[Duality Gap]]
- [[GPU CSR SpMV]]

## Referenced By

- [[Lu-2025-cuPDLP-GPU-Implementation|research/papers/Lu-2025-cuPDLP-GPU-Implementation]]