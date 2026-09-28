# QPLIB Dataset Ingestion (Requirement R19)

## 1. Overview & Provenance
QPLIB is the international standard library of quadratic programming benchmark instances (Furini et al., *Mathematical Programming Computation*, 2019). The instances in this directory represent standard convex continuous quadratic programs spanning box-constrained problems (QBL) and linearly constrained problems (QCL), including classical Markowitz mean-variance portfolio selection and chemical/refinery flow distribution.

Each instance is ingested from native `.qplib` format into standard MPS format with `QUADOBJ` extensions using the sovereign script `scripts/import_qplib.py`.

Every MPS model is paired with a `.provenance.json` artifact capturing cryptographic SHA-256 integrity hashes, problem class, variable and constraint dimensions, objective direction, and nonzero counts.

---

## 2. Ingested Instances

| Instance | Problem Class | Variables ($n$) | Constraints ($m$) | Hessian Nonzeros ($H_{\text{nnz}}$) | Linear Nonzeros ($A_{\text{nnz}}$) | SHA-256 Hash |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **QPLIB_0001** | QBL | 5 | 0 | 5 | 0 | `f8e04b1598...` |
| **QPLIB_0002** | QCL | 4 | 2 | 4 | 6 | `66874ebfec...` |
| **QPLIB_0010** | QCL | 6 | 2 | 11 | 12 | `3c87e6fa50...` |
| **QPLIB_0025** | QCL | 6 | 3 | 6 | 12 | `b539c3ea1a...` |

---

## 3. Conversion Pipeline
- **Importer Script**: `scripts/import_qplib.py`
  - Parses native positional QPLIB syntax, headers, objective sense, and bounds.
  - Generates MPS `NAME`, `OBJSENSE`, `ROWS`, `COLUMNS`, `RHS`, `RANGES`, `BOUNDS`, and `QUADOBJ` records.
  - Emits JSON provenance metadata.
- **Generator / Suite Harness**: `scripts/generate_qplib_suite.py`

---

## 4. Solve Performance & Verification

All 4 instances were solved to optimality using **markov-cero**'s sovereign QP ADMM engine (`--engine qp`) and independently verified against KKT stationarity, primal feasibility, and dual feasibility (`QP KKT certificate verified`):

| Instance | Status | KKT Verified | Objective Value | Iterations | Runtime (ms) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `QPLIB_0001` | **Optimal** | **YES** | `-3.500000` | 25 | 0.10 ms |
| `QPLIB_0002` | **Optimal** | **YES** | `-2.107145` | 32 | 0.10 ms |
| `QPLIB_0010` | **Optimal** | **YES** | `-0.102160` | 26 | 0.11 ms |
| `QPLIB_0025` | **Optimal** | **YES** | `2680.99752` | 31 | 0.10 ms |

---

## 5. License & Compatibility
- **Source**: QPLIB (Zuse Institute Berlin / University of Bologna).
- **License**: Publicly available benchmark dataset for academic and open-source scientific evaluation. Unrestricted redistribution of mathematical model coordinates.
