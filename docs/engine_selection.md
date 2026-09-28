# Engine Selection — W6 Classifier Output

Status: authoritative reference for the problem classifier introduced in W6.
The decision tree and thresholds below are **LOCKED** by the implementation plan
(W6, section 6.1). Changing a threshold requires new benchmark evidence and
explicit owner approval; a future implementer who measures different thresholds
must document the result and ask the owner before touching these constants.

## Classification decision tree (locked)

Evaluated top-down; the first matching branch wins.

```
Has NLP callbacks (NlpModel non-null functions)?
  YES → has integer variables? → MINLP : NLP
  NO  →
    Has NLOBJ section in MPS?
      YES → has integer variables? → MINLP : NLP
      NO  →
        Has quadratic matrix (QUADOBJ/QMATRIX, or non-empty quadratic_matrix)?
          YES → has integer variables? → MIQP : QP
          NO  →
            Has integer variables (VariableType::integer or VariableType::binary)?
              YES → MILP
              NO  → LP
```

Implementation: `src/model/classifier.cpp` (`classify_model`), header
`include/markov_cero/model/classifier.hpp`. Every solve records
`problem_class`, the matched `classification_reason`, and the
`recommended_backend` in the solve JSON.

## Engine auto-selection table (locked)

| Class | Default engine | Condition override |
|---|---|---|
| LP | `primal` (simplex) | NNZ > 50,000 → `pdlp` |
| LP (pdlp selected) | `pdlp --backend cpu` | `--backend gpu` AND NNZ > 500,000 → GPU PDLP |
| MILP | `milp` (single thread) | `--threads > 1` → `parallel` |
| QP | `qp` (ADMM CPU) | NNZ of P > 100,000 AND `--backend gpu` → GPU ADMM |
| MIQP | `miqp` | Always |
| NLP | `sqp` | Always |
| MINLP | `outer_approx` | Always |

## Locked thresholds (`model::EngineThresholds`)

| Constant | Value | Meaning |
|---|---|---|
| `kLpPdlpNnzThreshold` | 50,000 | LP auto-dispatch switches primal simplex → PDLP strictly above this structural NNZ count (at exactly 50,000, `primal` is kept). |
| `kPdlpGpuNnzThreshold` | 500,000 | Auto-dispatched PDLP may run on the GPU only above this NNZ count **and** only with an explicit `--backend gpu` request. |
| `kQpGpuNnzThreshold` | 100,000 | QP may use the GPU ADMM kernel only above this quadratic-matrix NNZ count **and** only with an explicit `--backend gpu` request. |

## Rules the classifier enforces

1. **Explicit engine wins.** When the caller passes any engine other than
   `auto`, the classifier still runs and records `problem_class`, but the engine
   choice is the caller's. The locked thresholds gate *auto* dispatch only.
2. **GPU never activates implicitly.** `recommended_backend` is `cpu` unless the
   caller explicitly requested `--backend gpu` **and** the instance is above the
   relevant threshold (plan W3/W4: below the threshold the H2D transfer makes
   the GPU slower; without a CUDA device the solver falls back to CPU).
3. **MILP parallelism is opt-in.** The classifier reports `milp`; the API layer
   upgrades to `parallel` only when `--threads > 1` was requested.
4. **No silent failure.** The classification reason string is always emitted so
   an evaluator can see *why* an engine was chosen.
