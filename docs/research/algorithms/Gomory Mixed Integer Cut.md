---
type: concept
tags: [algorithms, milp]
status: stable
verified_on: 2026-09-25
---

# Gomory Mixed Integer Cut

> Read a fractional tableau row and turn its remainders into an inequality no integer point can violate — the classical cut that modern solvers still run at the root.

## Definition
For a basic variable with fractional value, take its tableau row x_{n+1} + Σ ā_j x_j = ā₀, write f_j = frac(ā_j) and f₀ = frac(ā₀), and derive the Gomory mixed-integer cut Σ_{j: f_j ≤ f₀} (f_j/f₀) x_j + Σ_{j: f_j > f₀} ((1−f_j)/(1−f₀)) x_j ≥ 1 (for 0-1 variables; the general-integer form uses ⌊·⌋ terms). Every integer solution satisfying the original row satisfies the cut, so validity is a floor-function argument, not an empirical check. Cuts are only useful when f₀ is meaningfully fractional — hence a minimum-fractionality filter — and are generated in original-variable space after mapping back from the canonical model.

## Why It Matters Here
- R5 (cutting planes) and R13 (difficult MI relaxations) are the target; GMI is one of the two families actually implemented.
- Observed state: `generate_gomory_cuts` builds tableau rows ā = yᵀA, skips rows with f₀ outside [min_fractionality, 1−min_fractionality], maps coefficients through `OriginalVariableMap`, keeps a cut only if it has nonzeros and cuts the incumbent, then `filter_cuts` caps at 10 with min violation 1e-4 (src/milp/gomory.cpp:12-213).
- Observed state: separation happens at the root only, followed by a single re-solve (src/milp/milp_solver.cpp:174-205).

## Key Facts / Rules
- Cut: Σ_{f_j ≤ f₀}(f_j/f₀)x_j + Σ_{f_j > f₀}((1−f_j)/(1−f₀))x_j ≥ 1 (0-1 form).
- Validity for all integers follows from f₀ = frac(ā₀) rounding arguments; fractional f₀ is required.
- Numerical sensitivity: tableau entries near integers produce near-zero coefficients — filter by violation and re-check in original space.
- Gomory cuts were revived empirically (Balas et al. 1996) after being dismissed as weak in early implementations.

## Related
- [[Mixed Integer Rounding Cut]]
- [[Cut Validity]]
- [[Weak Relaxation]]
- [[CutGenerators]]
- [[Gomory-1963-All-Integer-Programming]]
- [[Balas-1996-Gomory-Cuts-Revisited]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[CutGenerators|codebase/components/CutGenerators]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Research-Code Traceability MOC|research/Research-Code Traceability MOC]]
- [[Branch and Cut|research/algorithms/Branch and Cut]]
- [[Mixed Integer Rounding Cut|research/algorithms/Mixed Integer Rounding Cut]]
- [[Cut Validity|research/concepts/Cut Validity]]
- [[Weak Relaxation|research/concepts/Weak Relaxation]]
- [[Cut Efficiency|research/metrics/Cut Efficiency]]
- [[Achterberg-2005-General-Mixed-Integer|research/papers/Achterberg-2005-General-Mixed-Integer]]
- [[Balas-1996-Gomory-Cuts-Revisited|research/papers/Balas-1996-Gomory-Cuts-Revisited]]
- [[Cornuejols-2008-Valid-Inequalities-Mixed|research/papers/Cornuejols-2008-Valid-Inequalities-Mixed]]
- [[Gomory-1958-Outline-Algorithm-Integer|research/papers/Gomory-1958-Outline-Algorithm-Integer]]
- [[Gomory-1963-All-Integer-Programming|research/papers/Gomory-1963-All-Integer-Programming]]
- [[Marchand-1996-Mixed-Integer-Rounding|research/papers/Marchand-1996-Mixed-Integer-Rounding]]
- [[Richard-2010-Group-Approach-Cutting|research/papers/Richard-2010-Group-Approach-Cutting]]
- [[Cut Pooling|research/techniques/Cut Pooling]]