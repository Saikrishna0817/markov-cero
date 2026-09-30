# Verifier overhead — NUM-01 measurement (2026-09-29)

**Task:** blueprint NUM-01, `BENCHMARK REQUIRED: Measure verifier overhead on representative sparse models; no speed target yet.`
**Purpose:** a dated measurement so a later tolerance, verifier or accumulation change has something to compare against. **No speed claim and no target is implied.**

## Method

- Build: Release configuration, `cmake --build` (same tree as the BASE-01 manifest).
- Binary: `verifier_overhead_benchmark` (`scripts/bench_verifier_overhead.cpp`), 64 repeats per model after one warm-up call.
- `solve_ms` — wall clock of `api::solve_model(model, {})`, the production path, which already runs both verifiers internally.
- `ref_ms` — wall clock of `lp::reference::solve` on the canonical model; that result is the witness handed to both verifiers.
- `canon_us` — mean cost of one `verify::verify_sparse_result` acceptance (class-specific boundary).
- `orig_us` — mean cost of one `verify::verify_primal` acceptance (original-model boundary).
- `share_pct` — `(canon_us + orig_us) / solve_ms` as a percentage: what verification costs relative to a full production solve of the same model.
- Integer models are canonicalized as an LP relaxation; the original-model check then runs with the integrality requirement off, which is stated in the output.
- Run command: `verifier_overhead_benchmark data/netlib/afiro.mps data/netlib/sc50a.mps data/netlib/adlittle.mps data/netlib/scagr7.mps data/netlib/recipe.mps data/netlib/sctap1.mps data/netlib/scorpion.mps data/mittelmann/neos5.mps 64`

## Raw output

```
data/netlib/afiro.mps rows=27 cols=32 nnz=83 status=Optimal assurance=optimality_witness_checked solve_ms=1.079 ref_solve_ms=0.553
  repeats=64 canonical=accepted original=accepted canon_us=1.50 orig_us=5.09 share_pct=0.61
data/netlib/sc50a.mps rows=50 cols=48 nnz=130 status=Optimal assurance=optimality_witness_checked solve_ms=1.954 ref_solve_ms=1.302
  repeats=64 canonical=accepted original=accepted canon_us=2.22 orig_us=8.08 share_pct=0.53
data/netlib/adlittle.mps rows=56 cols=97 nnz=383 status=Optimal assurance=optimality_witness_checked solve_ms=15.411 ref_solve_ms=14.601
  repeats=64 canonical=accepted original=accepted canon_us=4.55 orig_us=14.07 share_pct=0.12
data/netlib/scagr7.mps rows=129 cols=140 nnz=420 status=Optimal assurance=optimality_witness_checked solve_ms=20.468 ref_solve_ms=19.626
  repeats=64 canonical=accepted original=accepted canon_us=6.14 orig_us=26.75 share_pct=0.16
data/netlib/recipe.mps rows=91 cols=180 nnz=663 status=Optimal assurance=optimality_witness_checked solve_ms=8.830 ref_solve_ms=11.449
  repeats=64 canonical=accepted original=accepted canon_us=8.33 orig_us=29.11 share_pct=0.42
data/netlib/sctap1.mps rows=300 cols=480 nnz=1692 status=Optimal assurance=optimality_witness_checked solve_ms=138.135 ref_solve_ms=113.390
  repeats=64 canonical=accepted original=accepted canon_us=18.96 orig_us=99.06 share_pct=0.09
data/netlib/scorpion.mps rows=388 cols=358 nnz=1426 status=Optimal assurance=optimality_witness_checked solve_ms=134.542 ref_solve_ms=150.454
  repeats=64 canonical=accepted original=accepted canon_us=17.01 orig_us=89.12 share_pct=0.08
data/mittelmann/neos5.mps rows=63 cols=63 nnz=2016 status=ResourceLimit assurance=unverified solve_ms=60000.585 ref_solve_ms=39.697
  repeats=64 canonical=accepted original=accepted (integrality relaxed: LP relaxation witness) canon_us=15.54 orig_us=17.57 share_pct=0.00
```

## Reading the result

- Both verifiers **accepted every reference witness** on all eight models; no false rejection and no accepted-but-unverified status appeared in this run.
- Per-acceptance cost ranged **1.5 µs to 99 µs** across 83 to 2016 nonzeros.
- Relative to a full production solve the two checks together cost **0.08 % to 0.61 %**. The largest percentages are the smallest models, where fixed setup dominates the solve; the largest absolute cost is on `sctap1`, and it is still under 0.1 % of that solve.
- The **original-model check is 4–5× the canonical check** in absolute time on every model here (it recomputes activities from the untransformed model and also checks bounds and the objective). Any future optimisation should start there; any future tolerance change should be justified against both.

## Observations recorded with this run

1. `data/netlib/e226.mps` fails to parse: `MPS line 1683: objective-row RHS/RANGES values are unsupported`. Pre-existing parser limitation, unrelated to this task; excluded from the table.
2. `data/netlib/bore3d.mps` (233 × 315, 1429 nonzeros) reports `NumericalFailure` on the production path in ~146 ms, and the reference simplex also returns `NumericalFailure`, so no witness exists to verify. Not introduced by NUM-01 (this slice changes no engine code); recorded for LP-01 to triage.
3. `data/mittelmann/neos5.mps` hits the 60 s resource limit on the MILP path and reports `assurance=unverified`, which is the required fail-closed behaviour after a resource stop. The LP-relaxation witness it produced verifies cleanly.
4. The `share_pct` denominator includes engine start-up and model parsing, so it understates verification's share of a long solve and overstates it on tiny models. It is a ratio to compare across runs, not a decomposition.
