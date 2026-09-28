# Dispatch for Explorer Survey 3 (explorer_benchmarks_ml)

Working Directory: /home/saikrishna/markov-initial-build/.agents/explorer_survey_3
Role: teamwork_preview_explorer
Workspace Root: /home/saikrishna/markov-initial-build
Authoritative Request: /home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md

Task:
Perform a comprehensive survey of ML branching, datasets, benchmarks, and external solver comparison harness:
- Examine `src/milp/` and `src/milp/ml_branching/` (existing strong branching logger, GNN/ONNX inference, MIPLIB easy instances)
- Examine `data/` directory (existing datasets, Netlib, MIPLIB 2017, Mittelmann, QPLIB, ONNX models)
- Examine `scripts/` (existing download scripts, verification scripts, benchmarking scripts like `scripts/run_full_compare.py`)
- Check existing benchmark outputs and `evidence/` directory structure
- Check third-party solver availability in local system or scripts (HiGHS, GLPK, CBC, SCIP) and Mittelmann tables
- Check Python environment, packages (torch, onnx, pybind11, pytest, etc.) and build requirements

Deliverable:
Write a detailed report at `/home/saikrishna/markov-initial-build/.agents/explorer_survey_3/benchmark_ml_report.md` and `handoff.md`.
Report current state of datasets, ML branching pipeline, comparative harness, and gaps relative to ORIGINAL_REQUEST.md.

## 2026-09-26T19:15:47Z

Task:
Perform a comprehensive survey of ML branching, datasets, benchmarks, and external solver comparison harness:
- Examine src/milp/ and src/milp/ml_branching/ (existing strong branching logger, GNN/ONNX inference, MIPLIB easy instances)
- Examine data/ directory (existing datasets, Netlib, MIPLIB 2017, Mittelmann, QPLIB, ONNX models)
- Examine scripts/ (existing download scripts, verification scripts, benchmarking scripts like scripts/run_full_compare.py)
- Check existing benchmark outputs and evidence/ directory structure
- Check third-party solver availability in local system or scripts (HiGHS, GLPK, CBC, SCIP) and Mittelmann tables
- Check Python environment, packages (torch, onnx, pybind11, pytest, etc.) and build requirements

Report current state of datasets, ML branching pipeline, comparative harness, and gaps relative to ORIGINAL_REQUEST.md.

Write your detailed report to /home/saikrishna/markov-initial-build/.agents/explorer_survey_3/benchmark_ml_report.md and your handoff report to /home/saikrishna/markov-initial-build/.agents/explorer_survey_3/handoff.md.
When finished, send a completion message back to parent using send_message.
