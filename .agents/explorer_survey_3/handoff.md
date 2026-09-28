# Handoff Report: Survey of ML Branching, Datasets, and Comparative Benchmark Harness

**Agent ID**: `explorer_survey_3` (`teamwork_preview_explorer`)  
**Working Directory**: `/home/saikrishna/markov-initial-build/.agents/explorer_survey_3`  
**Handoff Type**: Hard (Task complete)  
**Timestamp**: 2026-09-26T19:32:00Z  

---

## 1. Observation

### 1.1 ML Branching Components & Codebase Inspection
- **MILP Core**: `include/markov_cero/milp/branch_selector.hpp` lines 10–40 define `BranchingStrategy` as `{ most_fractional, pseudo_cost, strong_branching, reliability }`. No `ml_gnn` enum exists.
- **CLI Options**: `apps/cli_options.hpp` lines 108–126 only handle `most_fractional`, `pseudo_cost`, `strong_branching`, and `reliability`. Any other argument produces `invalid branching strategy: <value>`.
- **Existing Strong Branching**: `src/milp/strong_branching.cpp` lines 13–64 implement `evaluate_strong_branching` using dual simplex (`lp::dual::solve`), computing Achterberg convex combination $(1 - \mu) \min + \mu \max$ with lookahead iteration caps and bound tightening.
- **Pseudo-Cost Baseline on `stein15.mps`**: In `evidence/miplib_results.csv` line 3:
  ```csv
  STEIN15,36,15,15,9.0,9,0.0,Optimal,True,True,157,31807,241,2,1161.944127,0,0,True
  ```
  Markov-cero pseudo-cost branching solves `stein15.mps` in **157 branch-and-bound nodes**.
- **Missing ML Directory**: Running `find_by_name` for `*ml_branching*` across `/home/saikrishna/markov-initial-build` returned **0 results**. Neither `src/milp/ml_branching/training_logger.cpp` nor `src/milp/ml_branching/onnx_scorer.cpp` exists.
- **Missing Model Artifact**: `data/ml_models/` does not exist; `data/ml_models/branching_scorer.onnx` is absent.
- **Sovereignty Guard Constraints**: `scripts/check-sovereignty.py` lines 18–23 specify:
  ```python
  FORBIDDEN_LIBRARIES = {
      "glpk", "gurobi", "cplex", "xpress", "xprs", "highs", "clp", "cbc",
      "coin", "osqp", "scs", "ipopt", "scip", "mosek", "eigen", "eigen3",
      "boost", "fmt", "spdlog", "gtest", "gmock", "googletest", "nlohmann",
      "torch", "libtorch", "onnx", "onnxruntime", "alglib", "ceres",
  }
  ```
  Lines 25–31 forbid `#include <torch/torch.h>` and `onnxruntime`. CTest #35 (`sovereignty_guard`) verifies this in CI.

### 1.2 Datasets & Ingestion Status
- **Netlib LP**: `data/netlib/` contains **17 files** (`adlittle`, `afiro`, `beaconfd`, `blend`, `kb2`, `lotfi`, `recipe`, `sc105`, `sc205`, `sc50a`, `sc50b`, `scagr7`, `scorpion`, `scsd1`, `scsd6`, `share1b`, `share2b`). `scripts/run_netlib.py` lines 20–93 only lists 12 instances in `NETLIB_BENCHMARKS`. 80 of 97 standard Netlib instances are missing.
- **MIPLIB**: `data/miplib/` contains **3 files** (`flugpl.mps`, `stein15.mps`, `stein9.mps`). The MIPLIB 2017 benchmark subset and easy instances are completely absent.
- **Mittelmann**: `data/mittelmann/` contains **6 files** (`bienst1.mps`, `bienst2.mps`, `markshare_5_0.mps`, `mkc1.mps`, `neos5.mps`, `ran14x18_1.mps`). In `evidence/mittelmann_results.csv`, all 6 failed or were resource-limited (`bienst1`, `bienst2`, `mkc1` hit `NumericalFailure`; `markshare_5_0`, `neos5`, `ran14x18_1` hit `ResourceLimit`). No download script exists.
- **QPLIB**: `data/qp/` contains **4 files** (`QPLIB_0001`, `0002`, `0010`, `0025`) and corresponding `.provenance.json` files, transcoded from strings in `scripts/generate_qplib_suite.py`. No real download pipeline from QPLIB repository exists.

### 1.3 Comparison Harness & Benchmark Outputs
- **Existing Runner**: `scripts/run_compare.py` benchmarks against HiGHS 1.15.1 via `highspy` across 20 instances (`data/compare/netlib.txt` [17] and `data/compare/miplib.txt` [3]). It outputs to `evidence/compare/results.csv`, `evidence/compare/report.md`, and `evidence/compare/profile.svg`.
- **Target Script**: `scripts/run_full_compare.py` does **not exist**.
- **External Solvers**: Local free solvers (GLPK, CBC, SCIP) and Mittelmann commercial tables (CPLEX, Gurobi, Xpress) are not supported in `scripts/run_compare.py`.
- **Output Path Mismatch**: R6 requires separate `dolan_more_lp.svg` and `dolan_more_milp.svg` in `evidence/comparison/` (plural). Currently only a single combined `profile.svg` is produced in `evidence/compare/` (singular).

### 1.4 System Toolchain & Environment
- C++ toolchains: `g++ (GCC) 16.2.1` and `clang 22.1.8`, `cmake 4.4.3`, `ctest`.
- Baseline CTest run: `ctest --test-dir /home/saikrishna/markov-initial-build/build -j4` ran 59 tests in 30.05 seconds, **100% passed (59/59)**.
- Python environment: System default is Python 3.14.7. `uv` is installed at `/home/saikrishna/.local/bin/uv`. Python 3.12 and 3.11 runtimes are available.
- Packages: `torch 2.14.0`, `onnx 1.23.0`, `onnxruntime 1.30.0`, `pybind11 3.1.0`, `pytest 9.1.1`, `scikit-learn 1.9.1` resolve and install cleanly via `uv`.
- Solvers: `highspy 1.15.1` is installed in `.compare-venv`. Solvers `pyscipopt`, `mip` (bundles CBC), `pulp` (bundles CBC), `swiglpk` install cleanly via `uv`. Arch pacman packages `glpk-5.0-3`, `coin-or-cbc-2.10.13`, `scip-10.0.3` can also be unpacked into user directories without root.

---

## 2. Logic Chain

1. **Premise**: Milestone 5 (R5) requires an ML-assisted branching pipeline with strong branching data logging, strict ML best practices, 2-layer bipartite GCN training, Int8 ONNX export to `data/ml_models/branching_scorer.onnx`, zero-dependency C++ inference (`onnx_scorer.cpp`), and equal or fewer B&B nodes on `stein15.mps` ($\le 157$ nodes).
2. **Observation**: `src/milp/strong_branching.cpp` currently produces strong branching scores, and `stein15.mps` solves in 157 nodes. However, `src/milp/ml_branching/` and `data/ml_models/` do not exist.
3. **Inference**: A data logging interface must be implemented to capture bipartite graph states during strong branching on MIPLIB easy instances. An offline Python training script using PyTorch must train the GCN with strict instance-based train/val/test splits (70/15/15) fitted normalizers, ranking metrics (Kendall's $\tau$, NDCG@k), and MSE.
4. **Sovereignty Constraint**: Because `scripts/check-sovereignty.py` forbids linking `libtorch` or `onnxruntime` in C++, the C++ inference engine (`onnx_scorer.cpp`) must be a sovereign, pure C++20 Int8 matrix-vector / tensor convolution evaluator.
5. **Premise**: Milestone 6 (R6) requires full Netlib (97 instances), MIPLIB 2017 easy/benchmark subset, Mittelmann suites, and convex QPLIB with SHA-256 provenance JSONs, plus an automated comparative harness `scripts/run_full_compare.py` against HiGHS, GLPK, CBC, SCIP, and commercial Mittelmann reference tables.
6. **Observation**: Currently, `data/netlib/` has only 17 files, `data/miplib/` has 3 files, `data/mittelmann/` has 6 failing files, `data/qp/` has 4 synthetic files, and `scripts/run_full_compare.py` does not exist.
7. **Inference**: Download and verification scripts must be created for each dataset suite. `scripts/run_full_compare.py` must be authored to drive multi-solver comparisons (leveraging `highspy`, Python-bundled CBC, `swiglpk`/pacman GLPK, and `pyscipopt`) alongside Mittelmann tables, outputting publication-grade Dolan-Moré curves (`dolan_more_lp.svg`, `dolan_more_milp.svg`) in `evidence/comparison/`.

---

## 3. Caveats

1. **Host CUDA Capability**: NVIDIA RTX 2050 GPU is present on host, but `nvcc` is not installed on system PATH. GPU builds (`MARKOV_CERO_ENABLE_CUDA`) are currently disabled in tree. ML training can run on CPU or PyTorch CUDA runtime, while C++ inference is strictly CPU-side zero-dependency C++20.
2. **Commercial Solvers (Gurobi, CPLEX, Xpress)**: Commercial proprietary solvers are not installed locally. The comparison harness must benchmark against published Mittelmann reference tables (from `http://plato.asu.edu/ftp/milp.html` and `lpsimpl.html`), which is the standard accepted evaluation methodology in SIH / academic benchmarks.
3. **MIPLIB 2017 Size**: The full MIPLIB 2017 collection is ~20 GB. Ingesting the designated "easy" instances collection (~40 instances) is appropriate and sufficient for training and verifying ML branching without exceeding disk and runtime budgets.

---

## 4. Conclusion

The existing codebase provides a solid, sovereign C++20 mathematical optimization core with 100% passing tests (59/59) and functional strong branching and comparison baselines against HiGHS. 

However, **Milestone 5 (ML-Assisted Branching, W2)** and **Milestone 6 (Full Datasets & Comparative Harness, W8 + W9)** require new components:
1. **ML Branching**: Implement `src/milp/ml_branching/training_logger.cpp`, an offline PyTorch GCN training script adhering to ML best practices, quantized Int8 ONNX export to `data/ml_models/branching_scorer.onnx`, zero-dependency C++ inference in `src/milp/ml_branching/onnx_scorer.cpp`, and `--branching ml_gnn` CLI support achieving $\le 157$ nodes on `stein15.mps`.
2. **Datasets**: Implement automated download & SHA-256 provenance verification scripts for Netlib LP (97 instances), MIPLIB 2017 easy instances, Mittelmann suites, and convex QPLIB.
3. **Comparative Harness**: Implement `scripts/run_full_compare.py` benchmarking markov-cero against HiGHS, GLPK, CBC, SCIP, and Mittelmann reference tables, generating `dolan_more_lp.svg` and `dolan_more_milp.svg` in `evidence/comparison/`.

The complete architectural survey, technical metrics, and gap analysis are fully documented in `/home/saikrishna/markov-initial-build/.agents/explorer_survey_3/benchmark_ml_report.md`.

---

## 5. Verification Method

To independently verify the facts and findings in this report:

1. **Verify Existing CTest Targets**:
   ```bash
   cd /home/saikrishna/markov-initial-build
   ctest --test-dir build -j4
   ```
   *Expected outcome*: 59 tests passed, 0 failures.

2. **Verify Branching Strategies and Baseline Node Count on `stein15.mps`**:
   ```bash
   build/markov-cero-solve data/miplib/stein15.mps --branching pseudo_cost
   cat evidence/miplib_results.csv | grep STEIN15
   ```
   *Expected outcome*: `nodes_explored` = 157. Confirm that running with `--branching ml_gnn` currently outputs `invalid branching strategy: ml_gnn`.

3. **Verify Absence of ML Branching Code & Models**:
   ```bash
   ls -d src/milp/ml_branching data/ml_models 2>&1
   ```
   *Expected outcome*: `No such file or directory`.

4. **Verify Sovereignty Guard Enforcement**:
   ```bash
   python3 scripts/check-sovereignty.py . --binary build/markov-cero-solve
   ```
   *Expected outcome*: "Sovereignty verification passed: zero external solver dependencies".

5. **Verify Python & Package Resolution via `uv`**:
   ```bash
   uv pip install --python /home/saikrishna/.compare-venv/bin/python --dry-run torch onnx onnxruntime pybind11 pytest
   ```
   *Expected outcome*: Clean package resolution without errors.

6. **Verify Dataset Counts**:
   ```bash
   ls data/netlib/*.mps | wc -l    # Output: 17 (expected 97)
   ls data/miplib/*.mps | wc -l    # Output: 3 (expected MIPLIB 2017 subset)
   ls data/mittelmann/*.mps | wc -l # Output: 6
   ls data/qp/*.mps | wc -l        # Output: 4
   ```
