#include "ipm_internal.hpp"
namespace markov_cero::lp::interior {
using namespace detail_ipm;
namespace detail_ipm {
SparseCsc transpose_csc(const SparseCsc& A) {
    const std::size_t m = A.rows;
    const std::size_t n = A.columns;
    const std::size_t nnz = A.values.size();

    SparseCsc At;
    At.rows = n;
    At.columns = m;
    At.column_offsets.assign(m + 1, 0);
    At.row_indices.resize(nnz);
    At.values.resize(nnz);

    for (std::size_t p = 0; p < nnz; ++p) {
        ++At.column_offsets[A.row_indices[p] + 1];
    }
    for (std::size_t i = 0; i < m; ++i) {
        At.column_offsets[i + 1] += At.column_offsets[i];
    }

    std::vector<std::size_t> cur = At.column_offsets;
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t p = A.column_offsets[j]; p < A.column_offsets[j + 1]; ++p) {
            const std::size_t r = A.row_indices[p];
            const std::size_t dest = cur[r]++;
            At.row_indices[dest] = j;
            At.values[dest] = A.values[p];
        }
    }
    return At;
}
}

namespace detail_ipm {
std::vector<double> spmv(const SparseCsc& A, const std::vector<double>& x) {
    std::vector<double> y(A.rows, 0.0);
    for (std::size_t j = 0; j < A.columns; ++j) {
        const double xj = x[j];
        if (xj == 0.0) continue;
        for (std::size_t p = A.column_offsets[j]; p < A.column_offsets[j + 1]; ++p) {
            y[A.row_indices[p]] += A.values[p] * xj;
        }
    }
    return y;
}
}

namespace detail_ipm {
std::vector<double> spmv_t(const SparseCsc& A, const std::vector<double>& y) {
    std::vector<double> z(A.columns, 0.0);
    for (std::size_t j = 0; j < A.columns; ++j) {
        long double sum = 0.0L;
        for (std::size_t p = A.column_offsets[j]; p < A.column_offsets[j + 1]; ++p) {
            sum += static_cast<long double>(A.values[p]) * y[A.row_indices[p]];
        }
        z[j] = static_cast<double>(sum);
    }
    return z;
}
}

namespace detail_ipm {
SparseCsc compute_sparse_normal_matrix(
    const SparseCsc& A,
    const SparseCsc& At,
    const std::vector<double>& d,
    double delta ) {
    const std::size_t m = A.rows;
    SparseCsc M;
    M.rows = m;
    M.columns = m;
    M.column_offsets.reserve(m + 1);
    M.column_offsets.push_back(0);

    std::vector<double> accum(m, 0.0);
    std::vector<int> marker(m, -1);
    std::vector<std::size_t> active_rows;

    for (std::size_t k = 0; k < m; ++k) {
        active_rows.clear();
        const std::size_t at_start = At.column_offsets[k];
        const std::size_t at_end = At.column_offsets[k + 1];
        for (std::size_t p = at_start; p < at_end; ++p) {
            const std::size_t j = At.row_indices[p];
            const double s_val = At.values[p] * d[j];
            const std::size_t a_start = A.column_offsets[j];
            const std::size_t a_end = A.column_offsets[j + 1];
            for (std::size_t q = a_start; q < a_end; ++q) {
                const std::size_t i = A.row_indices[q];
                if (marker[i] != static_cast<int>(k)) {
                    marker[i] = static_cast<int>(k);
                    active_rows.push_back(i);
                    accum[i] = 0.0;
                }
                accum[i] += s_val * A.values[q];
            }
        }
        if (marker[k] != static_cast<int>(k)) {
            marker[k] = static_cast<int>(k);
            active_rows.push_back(k);
            accum[k] = delta;
        } else {
            accum[k] += delta;
        }

        std::sort(active_rows.begin(), active_rows.end());
        for (std::size_t r : active_rows) {
            // Products summed into accum[r] can cancel to exactly zero (e.g.
            // symmetric +/- coefficient pairs); an explicit zero would make
            // the CSC invalid for SparseLu's canonical-form validate. Skip it:
            // the entry carries no normal-equation information, and the
            // diagonal keeps its +delta regularizer.
            if (accum[r] == 0.0) {
                continue;
            }
            M.row_indices.push_back(r);
            M.values.push_back(accum[r]);
        }
        M.column_offsets.push_back(M.values.size());
    }
    return M;
}
}

namespace detail_ipm {
Direction newton_direction_sparse(
    const SparseCsc& a,
    const SparseCsc& M,
    const std::vector<double>& x,
    const std::vector<double>& s,
    const std::vector<double>& d,
    const std::vector<double>& r_p,
    const std::vector<double>& r_d,
    const std::vector<double>& tau,
    const SparseLu& factor) {
    const std::size_t m = a.rows;
    const std::size_t n = a.columns;

    std::vector<double> u(n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
        u[j] = tau[j] / s[j] - x[j] + d[j] * r_d[j];
    }
    const auto a_u = spmv(a, u);
    std::vector<double> rhs(m);
    for (std::size_t i = 0; i < m; ++i)
        rhs[i] = -r_p[i] - a_u[i];

    // Iterative refinement on normal equations: early exit at 1e-14
    const auto dy = factor.solve_refined(M, rhs, 2, 1e-14);
    const auto a_t_dy = spmv_t(a, dy);

    Direction out;
    out.dy = dy;
    out.dx.resize(n);
    out.ds.resize(n);
    for (std::size_t j = 0; j < n; ++j) {
        out.ds[j] = -a_t_dy[j] - r_d[j];
        out.dx[j] = tau[j] / s[j] - x[j] - d[j] * out.ds[j];
    }
    return out;
}
}

namespace detail_ipm {
double max_step(const std::vector<double>& v, const std::vector<double>& dv,
                double fraction_to_boundary) {
    double alpha = 1.0;
    for (std::size_t j = 0; j < v.size(); ++j)
        if (dv[j] < 0.0)
            alpha = std::min(alpha, -v[j] / dv[j]);
    return fraction_to_boundary * alpha;
}
}

}
