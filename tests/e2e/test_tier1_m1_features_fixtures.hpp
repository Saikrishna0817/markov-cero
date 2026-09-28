#pragma once
// markov-cero E2E Test Suite: Tier 1 - Milestone 1 Feature Coverage
// Verifies nominal (happy path) functionality for Milestone 1 features:
// Feature 1: SparseLU IPM Normal Equations
// Feature 2: PDLP Stagnation Detection
// Feature 3: PDLP Dual Simplex Crossover
// Feature 4: ADMM Adaptive Penalty Rho
// Feature 5: ADMM KKT Refactorization Counter
// Feature 6: Always-On Iterative Refinement
// Feature 7: Structured Numerical Diagnostics & Verification

#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/linalg/dense_lu.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/lp/interior/ipm.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/kkt.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include "e2e_test_framework.hpp"

#include <cmath>
#include <numeric>
#include <vector>

using namespace markov_cero;
using namespace markov_cero::testing;

// Helper: compute A * D * A^T where A is m x n sparse CSC and D is n x n diagonal
inline linalg::SparseCsc compute_normal_equations(const linalg::SparseCsc& A,
                                                  const std::vector<double>& diag_D) {
    const std::size_t m = A.rows;
    const std::size_t n = A.columns;
    std::vector<std::vector<double>> full_mat(m, std::vector<double>(m, 0.0));

    // A is m x n, D is n x n. A D A^T = sum_{k=0}^{n-1} D[k] * A[:, k] * A[:, k]^T
    for (std::size_t k = 0; k < n; ++k) {
        const double d_k = diag_D[k];
        const std::size_t start = A.column_offsets[k];
        const std::size_t end = A.column_offsets[k + 1];
        for (std::size_t p1 = start; p1 < end; ++p1) {
            const std::size_t i = A.row_indices[p1];
            const double val_i = A.values[p1];
            for (std::size_t p2 = start; p2 < end; ++p2) {
                const std::size_t j = A.row_indices[p2];
                const double val_j = A.values[p2];
                full_mat[i][j] += d_k * val_i * val_j;
            }
        }
    }

    // Add small regularization to ensure positive definiteness
    for (std::size_t i = 0; i < m; ++i) {
        full_mat[i][i] += 1e-8;
    }

    return linalg::SparseCsc::from_columns(m, full_mat);
}

// ---------------------------------------------------------------------------
// Feature 1: SparseLU IPM Normal Equations
// ---------------------------------------------------------------------------
