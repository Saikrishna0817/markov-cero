# BRIEFING — 2026-09-26T19:35:00Z

## Mission
Comprehensive survey of ML branching, datasets, benchmarks, and external solver comparison harness.

## 🔒 My Identity
- Archetype: teamwork_preview_explorer
- Roles: explorer, analyst, investigator
- Working directory: /home/saikrishna/markov-initial-build/.agents/explorer_survey_3
- Original parent: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Milestone: Survey & Exploration

## 🔒 Key Constraints
- Read-only investigation — do NOT implement
- Write only to own folder: /home/saikrishna/markov-initial-build/.agents/explorer_survey_3
- Do NOT place source code, tests, or data files in .agents/

## Current Parent
- Conversation ID: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Updated: 2026-09-26T19:35:00Z

## Investigation State
- **Explored paths**: `src/milp/`, `include/markov_cero/milp/`, `apps/`, `data/`, `scripts/`, `evidence/`, `tests/`, `CMakeLists.txt`, system packages/toolchains.
- **Key findings**:
  1. `src/milp/ml_branching/` and `data/ml_models/` do not exist. Existing strong branching works; pseudo-cost baseline on `stein15.mps` is 157 nodes.
  2. Zero-dependency C++ inference is mandated by `scripts/check-sovereignty.py` (no libtorch/onnxruntime).
  3. Datasets: Netlib has 17/97; MIPLIB has 3 classic instances (MIPLIB 2017 absent); Mittelmann has 6 failing instances; QPLIB has 4 synthetic instances.
  4. Comparison harness: `run_compare.py` benchmarks vs HiGHS only (20 instances). `run_full_compare.py` is absent.
  5. Local solvers: HiGHS in venv; CBC, SCIP, GLPK available via uv/Python or pacman without root.
  6. Python packages (`torch`, `onnx`, `onnxruntime`, `pybind11`, `pytest`) resolve cleanly via `uv`.
- **Unexplored areas**: None within assigned survey scope.

## Key Decisions Made
- Structured findings across 6 survey axes in `benchmark_ml_report.md`.
- Produced complete 5-component hard handoff in `handoff.md`.

## Artifact Index
- DISPATCH.md — Task assignment and log
- BRIEFING.md — Situational awareness working memory
- progress.md — Liveness heartbeat
- benchmark_ml_report.md — Comprehensive survey report
- handoff.md — 5-component handoff report
