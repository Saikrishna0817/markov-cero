#include "kkt_internal.hpp"
namespace markov_cero::qp {
using namespace detail_kkt;
bool KktSolver::build_kkt_matrix(const SparseSymmetricMatrix& P,
                                 const linalg::SparseCsc& A,
                                 double sigma,
                                 const std::vector<double>& rho,
                                 std::optional<std::chrono::steady_clock::time_point> deadline) {
    const auto expired = [&] {
        return deadline && std::chrono::steady_clock::now() >= *deadline;
    };
    std::size_t work = 0;
    n_ = P.dimension;
    m_ = A.rows;
    total_dim_ = n_ + m_;

    // Triplets for upper-triangular entries of K (row <= col)
    std::vector<std::map<std::size_t, double>> col_entries(total_dim_);

    // 1. P + sigma*I block in top-left (dimension n x n)
    for (std::size_t j = 0; j < n_; ++j) {
        if (expired()) return false;
        col_entries[j][j] += sigma;
        if (j < P.column_offsets.size() - 1) {
            const std::size_t start = P.column_offsets[j];
            const std::size_t end = P.column_offsets[j + 1];
            for (std::size_t k = start; k < end; ++k) {
                if ((++work & 1023U) == 0U && expired()) return false;
                const std::size_t i = P.row_indices[k];
                if (i <= j) {
                    col_entries[j][i] += P.values[k];
                }
            }
        }
    }

    // 2. A^T block in top-right (col n + i, row j) for i in [0, m), j in [0, n)
    for (std::size_t j = 0; j < n_; ++j) {
        if (expired()) return false;
        if (j < A.column_offsets.size() - 1) {
            const std::size_t start = A.column_offsets[j];
            const std::size_t end = A.column_offsets[j + 1];
            for (std::size_t k = start; k < end; ++k) {
                if ((++work & 1023U) == 0U && expired()) return false;
                const std::size_t i = A.row_indices[k];
                if (i < m_) {
                    col_entries[n_ + i][j] += A.values[k];
                }
            }
        }
    }

    // 3. -diag(rho)^-1 block in bottom-right (col n + i, row n + i)
    for (std::size_t i = 0; i < m_; ++i) {
        if ((++work & 1023U) == 0U && expired()) return false;
        const double r = (i < rho.size() && rho[i] > 0.0) ? rho[i] : 1e-3;
        col_entries[n_ + i][n_ + i] -= 1.0 / r;
    }

    // Assemble upper-triangular CSC representation
    kkt_col_ptr_.assign(total_dim_ + 1, 0);
    kkt_row_ind_.clear();
    kkt_val_.clear();

    for (std::size_t j = 0; j < total_dim_; ++j) {
        if (expired()) return false;
        for (const auto& [r, v] : col_entries[j]) {
            if (std::abs(v) > 1e-20) {
                kkt_row_ind_.push_back(r);
                kkt_val_.push_back(v);
            }
        }
        kkt_col_ptr_[j + 1] = kkt_val_.size();
    }
    return true;
}
}
