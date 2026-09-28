---
type: audit
phase: 17
title: SIH Demo Strategy
tags: [audit, sih, demo, video, deck, phase-17]
status: complete
date: 2026-09-25
---

# Phase 17 — SIH Demo Strategy

The 5–10 minute demo, the 6-slide deck and the non-AI video for SIH26119 (C5/C6 in
[[00-ground-truth]]). Scoring lens from [[10-sih-evaluator-report]] §7.7: today's honest demo is
≈4 minutes with no "wow"; award-grade needs the head-to-head, the robustness beat and an honest
GPU chart. Delivery order follows [[13-restart-point]] steps 1–7.

## 17.1 P0 blockers before the demo is safe to run

| # | Blocker | Source | Fix (step in [[13-restart-point]]) |
|---|---|---|---|
| B1 | **R16 comparison absent** — zero artifacts repo-wide; a single question kills the score | [[09-research-code-alignment]] §6.1, [[10-sih-evaluator-report]] 7.1 | Step 1: `scripts/run_compare.py --baseline highs` |
| B2 | **Claims audit** — "up to 29320×" and implied parallel speedup are contradicted by our own CSVs | [[10-sih-evaluator-report]] K2, [[12-keep-remove-rebuild]] §8.2 | Step 2: claims↔evidence table in `STATUS.md`; delete the headline |
| B3 | **BLEND numerical failure hidden** — `scripts/run_gpu.py:241-245` never checks `res_simplex["status"]`, so `gpu_benchmarks` passes anyway | `_deployment-phase2-build/gpu_benchmark.csv:3`, [[blend-numerical-failure]] | Step 2: fix the gate; drop `blend` from the GPU demo set until fixed |
| B4 | **Parallel claim is negative** — 4 threads = 0.56×, efficiency 14 % | [[negative-parallel-scaling]], [[10-sih-evaluator-report]] K5 | Step 4: RW-2 fix **or** default `--threads 1` and remove the claim |
| B5 | **No robustness dossier** — unit tests are not a demonstration | [[09-research-code-alignment]] §6.1 R17 | Step 5: `evidence/robustness-dossier.md` (E3) |

## 17.2 Beat-by-beat script (5–10 min)

| Time | Beat | Runs live? | Exact command / artifact |
|---|---|---|---|
| 0:00–0:30 | Cold open: "this solver was written from scratch — here is the proof" | **live** | `python3 scripts/check-sovereignty.py . --binary build/markov-cero-solve` → `third_party/` empty, NEEDED allowlist OK |
| 0:30–1:15 | Problem framing: SIH26119, what a solver *core* is, R1–R20 coverage table | slide (deck p.1) | `STATUS.md` coverage table |
| 1:15–2:30 | Build from a clean clone + first solve | **live** (build pre-warmed, commands typed) | `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j` then `./build/markov-cero-solve examples/blend.mps --output /tmp/blend.json` |
| 2:30–3:30 | Certificate display: independent verification, not a status string | **live** | `python3 -c "import json;d=json.load(open('/tmp/blend.json'));print(d['status'],d['verified'],d['canonical_verified'],d['original_verified'])"` → `Optimal True True True`; QP beat: `./build/markov-cero-solve examples/qp_portfolio.mps --engine qp --output /tmp/qp.json` → `QP KKT certificate verified` |
| 3:30–5:00 | **Head-to-head vs HiGHS** on the same MPS (time + objective + geometric means) | **live** (pre-seeded instance set) | `python3 scripts/run_compare.py --baseline highs --suites netlib miplib --output evidence/comparison/` then `column -t -s, evidence/comparison/table.csv` |
| 5:00–6:15 | Robustness: a degenerate + an ill-conditioned instance where a naive run stalls and markov-cero converges | **live**, 1 instance each | `./build/markov-cero-solve data/robustness/degenerate_klee_minty.mps` + `evidence/robustness-dossier.md` on screen |
| 6:15–7:15 | Benchmark breadth: Netlib / MIPLIB / Mittelmann results with GM + Dolan–Moré profile | recorded screenshot fallback, table shown live | `ctest --test-dir build -R "netlib_benchmarks\|miplib_benchmarks\|mittelmann_benchmarks"` and `evidence/comparison/profile.svg` |
| 7:15–8:15 | Honest GPU story: crossover chart **with hardware label**, incl. the scales where CPU wins | chart live, run recorded | `evidence/benchmarks/crossover_study.csv` + `evidence/hardware.md` |
| 8:15–9:00 | Cuts + parallel before/after — **only if RW-1/RW-2 landed**; otherwise this beat is cut entirely | conditional | `evidence/benchmarks/cut_reduction.csv`, `parallel_scaling.csv` |
| 9:00–10:00 | Close: sovereignty + certificates + comparison in one line; roadmap (R4/IPM honest status) | slide (deck p.6) | deck PDF |

**Offline wrapper (already exists):** `bash run-qualification-demo.sh` — prints problem, build,
engine route, solve, JSON result and exit code; it is the 1:15–3:30 segment pre-baked so a cold
machine still demos in under a minute.

**Recorded fallback:** every `live` cell above has a screen capture taken *after* the P0 blockers
close (`docs/codebase/tests/benchmark-suites.md` rule: nothing recorded before the fix). The
fallback sequence is the same script with beats played from `video/demo-capture.mkv`.

## 17.3 Story arc (five beats, in this order)

1. **Sovereignty** — provenance chain + CI guard on screen; this is the one thing no commercial
   vendor can show ([[10-sih-evaluator-report]] 7.2; [[clean-room-provenance]]).
2. **Correctness** — every solution ships an independent certificate (dual-gated canonical +
   original space, KKT residual); [[IndependentVerifiers]] is the differentiator.
3. **Comparison** — the HiGHS table with geometric means and a Dolan–Moré profile; closes R16
   (E1) and converts "we think we are fast" into a graded artifact.
4. **Robustness** — the dossier: degenerate, ill-conditioned, weak-relaxation instances where
   status stays Optimal and the residual stays ≤ tol (E3, R17).
5. **Honest GPU** — the crossover chart with the losing scales shown, hardware named, and the
   status-check bug disclosed as fixed. Admitting the boundary is the credibility move (K2).

## 17.4 6-slide PDF deck outline

**Slide 1 — Problem & sovereignty.** States SIH26119 in one sentence, then the claim that
distinguishes this submission: a clean-room C++20 solver core with no solver dependency, proven
in CI rather than asserted. One diagram of the from-scratch boundary (what is ours vs what is
deliberately absent) and the R1–R20 coverage strip. *Evidence: `PROVENANCE.md`, `sovereignty_guard`
CTest log, `evidence/hardware.md`.*

**Slide 2 — Architecture.** The target architecture from model → reduction → engines →
verification → evaluation as a single horizontal flow, annotated with the five solver families
(simplex, dual, PDLP, B&B, ADMM/LDLᵀ) and the extension points that make R3 structurally true.
Kept to one figure; algorithmic detail is deliberately not the story. *Evidence:
[[14-target-architecture]], `docs/codebase/components/`.*

**Slide 3 — Correctness you can check.** Explains the certificate: an independent verifier
recomputes primal/dual/KKT residuals in canonical *and* original space, and the JSON carries
`verified`, `canonical_verified`, `original_verified`. Includes one real certificate block from
the demo run and the Netlib relative-error column (≤ 7.9e-15). *Evidence:
`evidence/netlib_results.csv`, `evidence/local-verification-report.txt`.*

**Slide 4 — Head-to-head vs HiGHS.** The R16 slide: shared instance set, same hardware, status /
objective / time / gap per instance, geometric-mean row, and the Dolan–Moré profile chart.
Where markov-cero loses, say so on the slide. *Evidence: `evidence/comparison/table.csv`,
`evidence/comparison/profile.svg`, `evidence/comparison/geom_means.md` (E1).*

**Slide 5 — Robustness dossier.** Three instance families — degenerate, ill-conditioned,
weak-relaxation — each with condition/perturbation descriptor, status, runtime, KKT residual,
and the failure mode a naive method hits. This is the R17 answer and the reason R13 is more than
a unit test. *Evidence: `evidence/robustness-dossier.md` + `evidence/robustness/*.csv` (E3).*

**Slide 6 — Honest results & roadmap.** Benchmark breadth table (Netlib/MIPLIB/Mittelmann/QPLIB)
with GM, the CPU/GPU crossover chart with hardware labels and the losing region visible, the
cuts/parallel before-after *if measured*, and an explicit "what is not done yet" list (IPM status,
scale to millions of rows, ML branching deferred). *Evidence: `evidence/benchmarks/*.csv`,
`evidence/hardware.md`, `STATUS.md` coverage table.*

## 17.5 Non-AI video requirements (SIH rule C6)

- **Team narration is mandatory:** a named team member speaks over the screen capture in their
  own voice for the full 5–10 minutes; no synthetic/clone voice, no generated imagery, no
  AI-produced slides or animation — SIH disallows generated content (state the compliance in the
  repo, e.g. `video/AFFIDAVIT.md`).
- **Screen capture plan:** OBS (or equivalent) at 1920×1080/30 fps capturing the terminal, the
  deck and the JSON output; one take of the full live script from §17.2 plus one take of the
  "just the results" path; overlays limited to a burn-in timestamp and machine name.
- **Backup recording:** record a second pass immediately after the first while everything is
  still warm, and keep the screen capture of every benchmark command as separate clips so any
  beat can be re-cut without re-running the suite.
- **Provenance of the video itself:** commit the capture script, the take log and the AFFIDAVIT;
  raw footage kept out of git (large), hash listed in the release notes.

## 17.6 Backup plan — risk → fallback

| Risk if it fails live | Fallback |
|---|---|
| HiGHS not installed / network down during the comparison beat | Play `evidence/comparison/table.csv` + `profile.svg` recorded after B1 closed; say "recorded 20 min ago on this machine" |
| Build too slow or fails on the demo machine | `run-qualification-demo.sh` uses the pre-warmed `build/`; if the build is broken, show the CI run + `evidence/local-verification-report.txt` |
| BLEND or any instance throws `NumericalFailure` | Instance removed from the demo set by the `gpu_status_gate`/`run_compare` pre-flight; substitute the golden-set instance already in §17.2 |
| GPU driver missing on the stage machine | GPU beat switches to the recorded crossover chart + `evidence/hardware.md`; no live CUDA needed (R8 is a chart claim, not a live claim) |
| Parallel run slower than serial (K5) | Beat 8:15–9:00 is cut entirely; demo runs `--threads 1` throughout and the deck omits the scaling claim |
| Robustness instance not present / downloads blocked | Dossier PDF/markdown opened instead of a live solve; the CSV rows still show status + residual |
| Mittelmann/QPLIB data missing | Benchmark beat limits itself to Netlib+MIPLIB (already in `data/`); breadth table cites the artifact paths for the others |
| Deck PDF won't render / laptop issue | Deck also exported to PNG per slide and committed; presenter mode uses the PNGs |
| Demo exceeds 10 minutes | Pre-empt: stop after the comparison beat (R16 is the binary requirement) and jump to the close |

## Related

[[10-sih-evaluator-report]] · [[13-restart-point]] · [[14-target-architecture]] · [[00-ground-truth]] · [[04-evidence-inventory]] · [[blend-numerical-failure]] · [[missing-hardware-metadata-in-evidence]] · [[No External Baseline]]
