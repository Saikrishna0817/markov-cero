# Comprehensive Survey: ML Branching, Datasets, Benchmarks, and External Solver Comparison Harness

**Author**: `explorer_survey_3` (`teamwork_preview_explorer`)  
**Date**: 2026-09-26T19:30:00Z  
**Workspace Root**: `/home/saikrishna/markov-initial-build`  
**Reference Document**: `/home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md`  

---

## 1. Executive Summary

A comprehensive investigation of the `markov-cero` repository was conducted to assess the current implementation status, infrastructure readiness, and specific technical gaps regarding **Milestone 5 (ML-Assisted Branching with ML Best Practices, W2)** and **Milestone 6 (Full Datasets & Comprehensive Solver Comparison, W8 + W9)**.

### Core Survey Takeaways
1. **ML Branching Pipeline (Milestone 5 / W2)**:
   - **Current State**: Strong branching (`src/milp/strong_branching.cpp`) and pseudo-cost branching (`src/milp/branch_selector.cpp`) are fully functional and tested (CTest #21). On `stein15.mps`, pseudo-cost branching currently solves the MILP in **157 nodes** (`evidence/miplib_results.csv:3`).
   - **Gap**: The directory `src/milp/ml_branching/` does **not exist**. There is no data logger (`training_logger.cpp`), no GNN model, no exported Int8 ONNX model in `data/ml_models/branching_scorer.onnx`, no zero-dependency C++ inference engine (`onnx_scorer.cpp`), and `--branching ml_gnn` is rejected by `apps/cli_options.hpp`.
   - **Critical Constraint**: `scripts/check-sovereignty.py` (enforced via CTest #35) strictly forbids linking `torch`, `libtorch`, `onnx`, or `onnxruntime` into the C++ binary. The C++ inference runtime must be **completely zero-dependency**, implemented as sovereign C++20 matrix/tensor math over quantized weights.

2. **Benchmark Datasets (Milestone 6 / W8)**:
   - **Netlib LP**: Only **17 instances** exist in `data/netlib/`. Only 12 are mapped in `scripts/run_netlib.py`. **80 instances** of the full 97-instance Netlib suite are missing.
   - **MIPLIB**: Only **3 instances** exist in `data/miplib/` (`stein9.mps`, `stein15.mps`, `flugpl.mps`), all from classic legacy suites. The **MIPLIB 2017 benchmark subset / easy collection** is completely missing.
   - **Mittelmann**: Only **6 instances** exist in `data/mittelmann/`, all of which currently encounter `ResourceLimit` or `NumericalFailure` (`evidence/mittelmann_results.csv`). No automated download or verification scripts exist.
   - **QPLIB**: Only **4 synthetic instances** exist in `data/qp/` (`QPLIB_0001`, `0002`, `0010`, `0025`), transcoded via `scripts/import_qplib.py`. A real automated convex QPLIB download pipeline is missing.

3. **Comparative Harness & External Solvers (Milestone 6 / W9)**:
   - **Current State**: `scripts/run_compare.py` benchmarks markov-cero against **HiGHS only** (`highspy 1.15.1`) over a 20-instance pre-registered set (`data/compare/netlib.txt` [17] + `miplib.txt` [3]). It outputs to `evidence/compare/results.csv`, `report.md`, and a single `profile.svg`.
   - **Gap**: `scripts/run_full_compare.py` does **not exist**. There is no comparison harness against local free solvers (**GLPK**, **COIN-OR CBC**, **SCIP**) or against published **Mittelmann reference tables** (CPLEX, Gurobi, FICO Xpress).
   - **Directory & Artifact Gaps**: Requirement R6 requires separate Dolan-Moré performance profiles (`dolan_more_lp.svg`, `dolan_more_milp.svg`) in `evidence/comparison/` (plural). Currently only a single combined profile exists in `evidence/compare/` (singular).

4. **Environment & Toolchains**:
   - Compilers: `g++ (GCC) 16.2.1` and `clang 22.1.8`, `cmake 4.4.3`, `ctest` (all 59 existing test targets pass 100%).
   - Python: `/usr/bin/python3` is Python 3.14.7. `uv` package manager is present at `/home/saikrishna/.local/bin/uv`. Python 3.12 and 3.11 runtimes are available.
   - Package Resolution: `torch 2.14.0`, `onnx 1.23.0`, `onnxruntime 1.30.0`, `pybind11 3.1.0`, `pytest 9.1.1`, `scikit-learn 1.9.1` resolve and install cleanly in Python 3.12 and 3.14.
   - Third-Party Solvers: `highspy 1.15.1` is pre-installed in `.compare-venv`. Solvers `pyscipopt`, `pulp` (bundles CBC), `mip` (bundles CBC), and `swiglpk` (GLPK) are directly installable via `uv` without root privileges. Furthermore, standalone Arch Linux solver packages (`glpk`, `coin-or-cbc`, `scip`, `highs`) can be downloaded and unpacked locally into user directories without root.

---

## 2. Detailed Survey of MILP & ML Branching Pipeline

### 2.1 Current In-Tree MILP Branching Substrate
The MILP solver components reside in `include/markov_cero/milp/` and `src/milp/`:
- **`include/markov_cero/milp/branch_selector.hpp` & `src/milp/branch_selector.cpp`**:
  - `BranchingStrategy` enum: `most_fractional`, `pseudo_cost`, `strong_branching`, `reliability`.
  - `VariablePseudoCost`: Tracks directional degradations (`down_sum`, `up_sum`, `down_count`, `up_count`) and calculates unit pseudo-costs.
  - `select_pseudo_cost`: Computes Achterberg product score $(\Delta^- + \epsilon)(\Delta^+ + \epsilon)$ with global fallback averages.
- **`include/markov_cero/milp/strong_branching.hpp` & `src/milp/strong_branching.cpp`**:
  - `evaluate_strong_branching`: Solves tentative down and up branch LPs using dual simplex (`lp::dual::solve`), warm-started with the current basis state.
  - Implements lookahead limits (`max_lookahead_iterations`, default 30), Achterberg convex score combination $\mu \max + (1 - \mu) \min$ ($\mu = 0.16$), and automatic domain reduction / bound fixing upon encountering infeasible branches.
  - Automatically updates `VariablePseudoCost` counters during strong branching evaluations.
- **`src/milp/milp_solver.cpp`**:
  - In `solve_milp`: Lines 237–273 run strong branching at the root node to initialize pseudo-costs and tighten bounds.
  - In branch-and-cut loop: Lines 547–580 select branching variables via the configured `branching_strategy`.
- **`src/milp/parallel_tree_search.cpp`**:
  - Multithreaded work-stealing tree search uses `select_branching_variable` to branch on fractional candidates.

### 2.2 Missing ML Branching Infrastructure (`src/milp/ml_branching/`)
The directory `src/milp/ml_branching/` is currently **completely absent**. The following components required by Milestone 5 (W2) must be designed and implemented:

1. **`src/milp/ml_branching/training_logger.cpp` & `include/markov_cero/milp/ml_branching/training_logger.hpp`**:
   - Must hook into the branch-and-bound node evaluation (specifically during strong branching lookahead on MIPLIB 2017 easy instances).
   - Must extract the bipartite graph representation $\mathcal{G} = (\mathcal{V}_c, \mathcal{V}_v, \mathcal{E})$:
     - **Constraint features** ($d_c = 4$): normalized RHS $b_i$, row sense ($\le, =, \ge$), dual activity, cosine angle with objective.
     - **Variable features** ($d_v = 6$): objective coefficient $c_j$, fractional part $f_j = x_j^* - \lfloor x_j^* \rfloor$, pseudo-cost up/down ratios, lower/upper bounds, variable integrality type.
     - **Edge attributes**: Non-zero constraint matrix coefficients $A_{ij}$ and incident connectivity.
   - Ground-truth target: Exact strong branching score $s_j^* = (1 - \mu) \min(\Delta^-, \Delta^+) + \mu \max(\Delta^-, \Delta^+)$ or Achterberg product score for each candidate.
   - Output format: Self-contained JSON / CSV / NPZ trace dataset.

2. **GNN Architecture & Training Pipeline**:
   - Model: 2-layer bipartite Graph Convolutional Network (GCN) matching Zhang et al. (2025) / Kimiaei et al. (2025) with ~20,000 parameters.
   - Message passing: Constraint-to-variable and variable-to-constraint feature transformations with ReLU / LeakyReLU activations.
   - Scoring head: Multi-layer perceptron projecting variable embeddings to scalar branching priority scores $\hat{s}_j$.
   - Quantization: Export to **Int8 quantized ONNX** (`data/ml_models/branching_scorer.onnx`).

3. **`src/milp/ml_branching/onnx_scorer.cpp` & `include/markov_cero/milp/ml_branching/onnx_scorer.hpp`**:
   - **Zero-Dependency C++ Inference**: Must parse the Int8 ONNX model (or an embedded Int8 weight tensor header) and execute bipartite graph convolutions using pure C++20 standard library math (vector dot products, SIMD-friendly loops).
   - Must **NEVER** link `onnxruntime`, `libtorch`, or external linear algebra libraries, preserving the clean-room sovereignty mandate.
   - Must implement `IBranchingScorer` interface (from `docs/audit/22-post-sih-architecture.md` §2.2).

4. **CLI and Solver Options Hook**:
   - Add `BranchingStrategy::ml_gnn` to `include/markov_cero/milp/branch_selector.hpp`.
   - Update `apps/cli_options.hpp` to parse `--branching ml_gnn`.
   - Wire `ml_gnn` into `src/milp/milp_solver.cpp` variable selection.

5. **Target Acceptance Criterion**:
   - Benchmark `stein15.mps`: With pseudo-cost branching, markov-cero currently explores **157 nodes** (`evidence/miplib_results.csv`).
   - The ML-assisted branching model must achieve **$\le 157$ nodes** on `stein15.mps`.

---

## 3. Detailed Survey of Benchmark Datasets (`data/`)

| Dataset Suite | Instances Present | Missing / Target Instances | Current Status & Behavior |
| :--- | :--- | :--- | :--- |
| **Netlib LP** | **17** in `data/netlib/` (`afiro`, `adlittle`, `beaconfd`, `blend`, `kb2`, `lotfi`, `recipe`, `sc105`, `sc205`, `sc50a`, `sc50b`, `scagr7`, `scorpion`, `scsd1`, `scsd6`, `share1b`, `share2b`) | **80 instances** (Full suite has 97 standard Netlib problems, e.g. `25fv47`, `brandy`, `czprob`, `d6cube`, `etamacro`, `fit1p`, `greenbea`, `grow15`, `israel`, `maros`, `pilot`, `sierra`, `stair`, etc.) | 15/17 solve to certified optimality in `run_compare.py`. `lotfi` and `beaconfd` stall in PDLP without crossover. Extended download dictionary in `run_netlib.py` only defines 12 problems. |
| **MIPLIB 2017** | **3** in `data/miplib/` (`flugpl.mps`, `stein15.mps`, `stein9.mps`) | **MIPLIB 2017 Benchmark Subset** (~240 instances) or **MIPLIB Easy Collection** (~40 small instances, e.g. `air04`, `bell5`, `dcmulti`, `egout`, `fixnet6`, `gen`, `l152lav`, `mas74`, `misc07`, `p0201`, `pk1`, `pp08a`, `rgn`, `vpm2`). | All 3 legacy instances solve cleanly (`flugpl`: 1813 nodes, `stein15`: 157 nodes, `stein9`: 17 nodes). MIPLIB 2017 collection is entirely uningested. |
| **Mittelmann** | **6** in `data/mittelmann/` (`bienst1.mps`, `bienst2.mps`, `markshare_5_0.mps`, `mkc1.mps`, `neos5.mps`, `ran14x18_1.mps`) | Full Mittelmann LP & MILP benchmark suites; reference result tables for commercial/open solvers. | 0/6 instances currently pass in markov-cero: 3 network LPs encounter `NumericalFailure` on singular basis factorization; 3 MILPs encounter `ResourceLimit` timeout. |
| **QPLIB** | **4** in `data/qp/` (`QPLIB_0001`, `0002`, `0010`, `0025`) | Convex QPLIB subset (e.g. `QPLIB_0001` through `QPLIB_0100` convex continuous instances). | 4 instances generated via hardcoded script strings in `generate_qplib_suite.py`. No network ingestion pipeline exists. |
| **ML Models** | **0** (`data/ml_models/` does not exist) | `data/ml_models/branching_scorer.onnx` (~20k parameter Int8 model) + weights metadata. | Completely absent. |

### Observations on Ingestion & Verification Scripts
- **Netlib**: `scripts/run_netlib.py` has an `ensure_instance()` function fetching `.mps.gz` from `https://raw.githubusercontent.com/coin-or-tools/Data-Netlib/master`. However, its dictionary `NETLIB_BENCHMARKS` only has 12 entries with known optimal objective values.
- **MIPLIB**: `scripts/run_miplib.py` has individual URLs for only 3 files from GitHub repositories (`ruppinlab/MORSE` and `coin-or-tools/Data-miplib3`).
- **Mittelmann**: No download script exists. Files were manually placed with a static `data/mittelmann/README.md`.
- **QPLIB**: `scripts/import_qplib.py` correctly parses positional `.qplib` text files and produces `.provenance.json` containing SHA-256 hashes. `scripts/generate_qplib_suite.py` creates 4 small synthetic instances.

---

## 4. Detailed Survey of Comparative Harness & Scripts

### 4.1 Current Comparison Harness (`scripts/run_compare.py`)
- **Structure**:
  - Compares `markov-cero-solve` against HiGHS via Python `highspy` (`.compare-venv/bin/python`).
  - Reads suite lists from `data/compare/<suite>.txt` (`netlib.txt` [17] and `miplib.txt` [3]).
  - Repeats each instance $N$ times (default 3, CTest uses 1), takes the median runtime.
  - Automatically executes a fallback retry with `--engine pdlp` if the default engine fails, accumulating total time.
  - Computes Geometric Mean of runtime ratios:
    $$\text{GM} = \exp\left( \frac{1}{|S|} \sum_{i \in S} \ln \max\left(\frac{t_{\text{mc}, i}}{t_{\text{hi}, i}}, 10^{-9}\right) \right)$$
  - Emits:
    - `evidence/compare/results.csv`: Table of status, objective values, runtimes, ratio, agreement.
    - `evidence/compare/report.md`: Markdown summary table with geometric mean ratio.
    - `evidence/compare/profile.svg`: Dolan-Moré performance profile curve in SVG.
- **Current Performance Baseline (from latest CTest run)**:
  - Total shared instances evaluated: 20 (17 Netlib, 3 MIPLIB).
  - markov-cero passed verification & objective agreement: **18/20 (90%)**.
  - Failures: `lotfi` and `beaconfd` (PDLP iteration limit reached without crossover).
  - Geometric-mean runtime ratio vs HiGHS: **30.82×**.

### 4.2 Gaps Relative to `ORIGINAL_REQUEST.md` (Milestone 6 / W9)
1. **Target Script**: ORIGINAL_REQUEST.md mandates an automated script named **`scripts/run_full_compare.py`**.
2. **Third-Party Solvers**:
   - The harness must benchmark markov-cero against multiple local free solvers:
     - **HiGHS** (already working via `highspy`)
     - **GLPK** (`glpsol` CLI or `swiglpk` Python module)
     - **COIN-OR CBC** (`cbc` CLI or `mip` / `pulp` Python bundled binary)
     - **SCIP** (`scip` CLI or `pyscipopt` Python module)
   - Must benchmark against **Mittelmann reference tables** for commercial solvers (**CPLEX**, **Gurobi**, **FICO Xpress**).
3. **Artifact Outputs & Locations**:
   - Target directory: `evidence/comparison/` (plural).
   - Target performance profiles: **`dolan_more_lp.svg`** (LP instances) and **`dolan_more_milp.svg`** (MILP instances) generated separately, rather than a single combined curve.
   - Target datasets: Full Netlib (97 instances), MIPLIB 2017 easy/benchmark subset, Mittelmann, and convex QPLIB.

---

## 5. Local Environment, Toolchain, and Solver Availability

### 5.1 Operating System & Hardware
- **OS**: Arch Linux (`Linux-7.2.5-3-omarchy-x86_64`, glibc 2.44).
- **CPU**: Intel Core i5-12450HX (8 cores / 12 threads).
- **RAM**: 11 GiB.
- **GPU**: NVIDIA GeForce RTX 2050 (4 GiB GDDR6, compute capability 8.6, driver 610.57.04).

### 5.2 Build Toolchain & Test Suite
- **C++ Compilers**:
  - `g++ (GCC) 16.2.1 20260810` (full C++20 support).
  - `clang 22.1.8` (full C++20 and libFuzzer support).
- **CMake & Build System**:
  - `cmake 4.4.3`.
  - `ctest` available.
  - `make` (`/usr/bin/make`) available. `ninja` is not installed.
- **CTest Baseline**:
  - Total tests currently registered: **59 tests**.
  - Current pass rate: **59/59 (100% pass)** in 30.05 seconds.
  - Tests covering branching & benchmarks:
    - Test #21: `strong_branching` (0.01s)
    - Test #54: `netlib_benchmarks` (0.20s)
    - Test #55: `miplib_benchmarks` (3.97s)
    - Test #56: `cut_effectiveness` (6.34s)
    - Test #57: `compare_harness` (8.75s)

### 5.3 Python Environments & Package Matrix
- **System Python**: `/usr/bin/python3` is **Python 3.14.7**.
- **Virtual Environment**: `/home/saikrishna/markov-initial-build/.compare-venv` uses Python 3.14.7 with `highspy 1.15.1`, `numpy 2.5.3`, `pip 26.2.1`.
- **Fast Package Manager (`uv`)**: Installed at `/home/saikrishna/.local/bin/uv`.
  - Provides Python 3.12 (`/home/saikrishna/.local/share/uv/python/cpython-3.12-linux-x86_64-gnu/bin/python3.12`) and Python 3.11.
- **ML & Testing Packages Resolution (Dry-Run Tested)**:
  - `torch 2.14.0`
  - `onnx 1.23.0`
  - `onnxruntime 1.30.0`
  - `pybind11 3.1.0`
  - `pytest 9.1.1`
  - `scikit-learn 1.9.1`, `scipy 1.18.1`, `pandas 3.0.6`
  All resolve cleanly and can be installed via `uv` in seconds.

### 5.4 Third-Party Solver Availability Analysis

| Solver | Package Channel | Availability on Host | Execution Method for Benchmark Harness |
| :--- | :--- | :--- | :--- |
| **HiGHS** | PyPI (`highspy 1.15.1`) / Pacman | **Installed** in `.compare-venv` | Direct Python in-process API via `highspy` (already used in `run_compare.py`). |
| **GLPK** | Pacman (`extra/glpk 5.0-3`) / PyPI (`swiglpk 5.0.13`) | Official Arch package available; Python wheel available | 1. Can run via `swiglpk` Python module; or<br>2. Unpack `glpk-5.0-3-x86_64.pkg.tar.zst` to user directory and call `glpsol` CLI. |
| **COIN-OR CBC** | Pacman (`extra/coin-or-cbc 2.10.13-2`) / PyPI (`mip`, `pulp`) | Python wheels `mip` and `pulp` bundle standalone CBC x86_64 binary | Call bundled `cbc` binary via Python `subprocess.run` (zero root / zero system install needed). |
| **SCIP** | Pacman (`extra/scip 10.0.3-2`) / PyPI (`pyscipopt 6.2.1`) | Python wheel `pyscipopt` available; Arch package downloadable | Call via `pyscipopt` in Python, or extract Arch package binary. |
| **Commercial (CPLEX, Gurobi, Xpress)** | Mittelmann Web Tables | Not installed locally (commercial proprietary) | Ingest published Mittelmann benchmark tables (`http://plato.asu.edu/ftp/milp.html`, `lpsimpl.html`) as immutable reference baselines. |

### 5.5 Sovereignty Guard Enforcements (`scripts/check-sovereignty.py`)
- The repository strictly enforces clean-room sovereignty:
  - `CMakeLists.txt` is scanned for disallowed `find_package()` and `target_link_libraries()`.
  - Source headers are scanned for disallowed includes (e.g. `glpk.h`, `Highs.h`, `scip.h`, `torch/torch.h`, `onnxruntime`).
  - Compiled binaries are inspected with `readelf -d` and `nm -D` to verify zero forbidden dynamic libraries or symbols.
- **Implication**: Any ML inference in C++ must be header-only or pure sovereign C++20. Third-party solvers must only be invoked externally by Python benchmark scripts across a process boundary.

---

## 6. ML Best Practices Compliance Plan (Milestone 5)

Per `ml-best-practices` skill instructions and `ORIGINAL_REQUEST.md`, Milestone 5 must strictly adhere to rigorous ML engineering protocols:

### 6.1 Strict Featurization Ordering
- **Partition First**: The strong branching dataset collected across MIPLIB instances must be partitioned **BEFORE** fitting any normalizers, standardizers, or feature encoders.
- **No Data Leakage**: Standard scalers (for node features, edge weights, and LP residuals) must be fitted exclusively on the training split, then applied to the validation and test splits.

### 6.2 Fixed Split Protocol
- The dataset must be split into:
  - **70% Training**
  - **15% Validation**
  - **15% Test**
- Splits must be partitioned **by instance** (not randomly shuffling node states across the same problem instance) to evaluate out-of-sample generalization across unseen MILP structures.

### 6.3 Outlier Handling & Target Transformation
- Branching degradations and objective improvements span several orders of magnitude ($10^{-6}$ to $10^8$ for infeasible branches).
- The training pipeline must apply logarithmic or robust quantile transformations:
  $$y = \log(1 + \max(0, s^*))$$
- Infeasible branch penalties must be capped with a designated upper bound sentinel to prevent gradient explosion.

### 6.4 Model Architecture & Metrics
- **Model**: 2-layer Bipartite Graph Convolutional Network (~20k parameters).
- **Dual Metric Evaluation**:
  - Regression error: Mean Squared Error (MSE), Mean Absolute Error (MAE).
  - **Ranking metrics**:
    - **Kendall's $\tau$ rank correlation**: Measures relative ranking agreement between predicted scores and true strong branching order.
    - **NDCG@k ($k \in \{1, 3, 5\}$)**: Evaluates whether the top predicted candidate matches the best strong branching variable.
  - Confusion matrix & classification metrics: Evaluate accuracy on identifying the top-1 branching candidate.
  - Latency profiling: Measure forward-pass inference latency in C++ to guarantee sub-millisecond execution.

### 6.5 Quantized Int8 Export & Sovereign Inference
- Export trained PyTorch model to ONNX.
- Apply Int8 dynamic quantization (`onnxruntime.quantization`).
- Implement zero-dependency C++ inference engine (`onnx_scorer.cpp`) that reads Int8 weights and executes matrix-vector products with integer arithmetic.

---

## 7. Traceability Gap Analysis Matrix Relative to ORIGINAL_REQUEST.md

| Requirement Item | Specified in ORIGINAL_REQUEST.md | Current Codebase Status | Gap Classification | Concrete Action Required |
| :--- | :--- | :--- | :--- | :--- |
| **R5.1 Logger** | Instrument strong branching data logger in `src/milp/ml_branching/training_logger.cpp` to record bipartite graph features and exact branch improvement scores on MIPLIB 2017 easy instances. | `strong_branching.cpp` exists and computes scores, but no logging or graph extraction code exists. `src/milp/ml_branching/` absent. | **Missing Component** | Create `src/milp/ml_branching/training_logger.cpp` and `include/markov_cero/milp/ml_branching/training_logger.hpp`. |
| **R5.2 ML Practices** | Fixed train (70%), validation (15%), test (15%) splits before fitting normalizers; handle outliers; evaluate Kendall's $\tau$, NDCG@k alongside MSE. | No ML training script or dataset split logic exists. | **Missing Component** | Implement offline training script (e.g. `scripts/train_branching_gnn.py`) with strict featurization ordering, ranking metrics, and confusion matrix. |
| **R5.3 GCN & ONNX** | Train a 2-layer bipartite GCN (~20k params), export to quantized Int8 ONNX (`data/ml_models/branching_scorer.onnx`). | No ONNX model in `data/ml_models/`. | **Missing Component** | Train GCN, quantize to Int8 ONNX, save to `data/ml_models/branching_scorer.onnx`. |
| **R5.4 C++ Inference** | Implement zero-dependency C++ inference (`src/milp/ml_branching/onnx_scorer.cpp`) active via `--branching ml_gnn`. | No `onnx_scorer.cpp`. `--branching ml_gnn` not supported by CLI parser. | **Missing Component** | Author zero-dependency C++ ONNX/quantized tensor inference in `src/milp/ml_branching/onnx_scorer.cpp`, wire into `apps/cli_options.hpp` and `milp_solver.cpp`. |
| **R5.5 Node Count Acceptance** | ML branching achieves equal or fewer branch-and-bound nodes than pseudo-cost branching on `stein15.mps`. | Pseudo-cost branching solves `stein15.mps` in 157 nodes. | **Verification Target** | Verify `markov-cero-solve data/miplib/stein15.mps --branching ml_gnn` explores $\le 157$ nodes. |
| **R6.1 Full Netlib** | Automate download and verification scripts for full Netlib LP (97 instances) with SHA-256 provenance JSONs. | 17 instances present in `data/netlib/`. `run_netlib.py` only defines 12 in `NETLIB_BENCHMARKS`. | **Data & Script Gap** | Author `scripts/download_netlib.py` to fetch all 97 instances with SHA-256 provenance JSONs. |
| **R6.2 MIPLIB 2017** | Automate download and verification scripts for MIPLIB 2017 benchmark subset with SHA-256 provenance JSONs. | Only 3 classic instances in `data/miplib/`. MIPLIB 2017 benchmark subset absent. | **Data & Script Gap** | Author `scripts/download_miplib.py` to ingest MIPLIB 2017 easy/benchmark instances with provenance. |
| **R6.3 Mittelmann Suite** | Automate download and verification scripts for Mittelmann suites with SHA-256 provenance JSONs. | 6 instances in `data/mittelmann/` without download script or provenance JSON. | **Data & Script Gap** | Author `scripts/download_mittelmann.py` to fetch instances and ingest reference tables. |
| **R6.4 Convex QPLIB** | Automate download and verification scripts for convex QPLIB instances with SHA-256 provenance JSONs. | 4 synthetic instances in `data/qp/`. No download script. | **Data & Script Gap** | Author `scripts/download_qplib.py` fetching real convex QPLIB instances. |
| **R6.5 Comparative Harness** | Author automated comparative harness (`scripts/run_full_compare.py`) benchmarking against local free solvers (HiGHS, GLPK, CBC, SCIP) and Mittelmann tables. | `run_compare.py` benchmarks against HiGHS only. `run_full_compare.py` absent. | **Script Gap** | Implement `scripts/run_full_compare.py` supporting HiGHS, GLPK, CBC, SCIP, and Mittelmann tables. |
| **R6.6 Dolan-Moré Profiles** | Generate Dolan-Moré performance profiles (`dolan_more_lp.svg`, `dolan_more_milp.svg`) and aggregate reports in `evidence/comparison/`. | Only single `profile.svg` in `evidence/compare/`. | **Output & Artifact Gap** | Update harness to emit separate `dolan_more_lp.svg` and `dolan_more_milp.svg` in `evidence/comparison/`. |

---

## 8. Recommended Implementation Roadmap for Builder Agents

To close these gaps systematically without destabilizing the existing 59 passing tests, the following execution plan is recommended:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ Phase A: Dataset Expansion & Download Infrastructure (W8)                             │
│ 1. Author scripts/download_netlib.py: Ingest all 97 Netlib LP instances with SHA-256   │
│ 2. Author scripts/download_miplib.py: Ingest MIPLIB 2017 easy instances with SHA-256    │
│ 3. Author scripts/download_mittelmann.py: Ingest Mittelmann instances & HTML tables   │
│ 4. Author scripts/download_qplib.py: Ingest convex QPLIB instances with provenance     │
└────────────────────────────────────────┬───────────────────────────────────────────────┘
                                         ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ Phase B: ML Branching Data Pipeline & Training (W2)                                   │
│ 1. Implement include/markov_cero/milp/ml_branching/training_logger.hpp & .cpp          │
│ 2. Run strong branching on MIPLIB easy instances to extract bipartite graph traces    │
│ 3. Implement scripts/train_branching_gnn.py:                                          │
│    - Fixed 70/15/15 instance split BEFORE normalization                                │
│    - 2-layer bipartite GCN (~20k params)                                               │
│    - Compute Kendall's tau, NDCG@k, MSE, confusion matrix                              │
│    - Export quantized Int8 ONNX to data/ml_models/branching_scorer.onnx               │
└────────────────────────────────────────┬───────────────────────────────────────────────┘
                                         ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ Phase C: Sovereign C++ Inference & Solver Integration                                  │
│ 1. Implement src/milp/ml_branching/onnx_scorer.cpp (Zero-dependency C++20 inference)  │
│ 2. Update BranchingStrategy enum & apps/cli_options.hpp with --branching ml_gnn       │
│ 3. Verify stein15.mps: nodes(ml_gnn) <= 157                                            │
│ 4. Run scripts/check-sovereignty.py to confirm zero forbidden libraries                │
└────────────────────────────────────────┬───────────────────────────────────────────────┘
                                         ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ Phase D: Full Comparative Harness (W9)                                                 │
│ 1. Setup local solver runners: HiGHS (highspy), CBC (pulp/mip), GLPK, SCIP             │
│ 2. Implement scripts/run_full_compare.py:                                             │
│    - Multi-solver benchmarking with geometric mean runtime ratios                      │
│    - Mittelmann reference table comparison (CPLEX, Gurobi, Xpress)                    │
│    - Separate Dolan-Moré curves: dolan_more_lp.svg & dolan_more_milp.svg              │
│ 3. Save full reports and profiles in evidence/comparison/                             │
└────────────────────────────────────────────────────────────────────────────────────────┘
```
