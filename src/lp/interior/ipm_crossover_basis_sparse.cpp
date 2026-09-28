#include "ipm_internal.hpp"
namespace markov_cero::lp::interior {
using namespace detail_ipm;
namespace detail_ipm {
std::optional<std::vector<std::size_t>> crossover_basis_sparse(
    const SparseCsc& a, const std::vector<double>& x_ipm) {
    const std::size_t m = a.rows;
    const std::size_t n = a.columns;
    const double xi = 1e-7;

    if (m == 0 || n < m)
        return std::nullopt;

    std::vector<std::size_t> large;
    std::vector<std::size_t> rest;
    for (std::size_t j = 0; j < n; ++j) {
        const double v = j < x_ipm.size() ? x_ipm[j] : 0.0;
        (v > xi ? large : rest).push_back(j);
    }
    std::sort(large.begin(), large.end(), [&](std::size_t lhs, std::size_t rhs) {
        return x_ipm[lhs] > x_ipm[rhs];
    });

    struct Pivot {
        std::size_t row;
        std::vector<double> column;
    };
    std::vector<Pivot> pivots;
    pivots.reserve(m);
    std::vector<char> row_used(m, 0);
    std::vector<std::size_t> basis;
    basis.reserve(m);

    const auto try_insert = [&](std::size_t j) {
        std::vector<double> c(m, 0.0);
        double norm = 0.0;
        for (std::size_t p = a.column_offsets[j]; p < a.column_offsets[j + 1]; ++p) {
            c[a.row_indices[p]] = a.values[p];
            norm = std::max(norm, std::abs(a.values[p]));
        }
        if (norm == 0.0)
            return;
        for (const Pivot& p : pivots) {
            const double factor = c[p.row];
            if (factor == 0.0)
                continue;
            for (std::size_t i = 0; i < m; ++i)
                c[i] -= factor * p.column[i];
        }
        const double threshold = 1e-10 * std::max(1.0, norm);
        std::size_t pivot_row = m;
        double best = threshold;
        for (std::size_t i = 0; i < m; ++i)
            if (!row_used[i] && std::abs(c[i]) > best) {
                best = std::abs(c[i]);
                pivot_row = i;
            }
        if (pivot_row == m)
            return;
        const double piv = c[pivot_row];
        for (std::size_t i = 0; i < m; ++i)
            c[i] /= piv;
        row_used[pivot_row] = 1;
        pivots.push_back(Pivot{pivot_row, std::move(c)});
        basis.push_back(j);
    };

    for (std::size_t j : large)
        if (basis.size() < m)
            try_insert(j);
    for (std::size_t j : rest)
        if (basis.size() < m)
            try_insert(j);

    if (basis.size() != m)
        return std::nullopt;

    // Verify basis with SparseLu
    std::vector<std::vector<double>> basis_cols(m, std::vector<double>(m, 0.0));
    for (std::size_t p = 0; p < m; ++p) {
        std::size_t col_idx = basis[p];
        for (std::size_t q = a.column_offsets[col_idx]; q < a.column_offsets[col_idx + 1]; ++q) {
            basis_cols[p][a.row_indices[q]] = a.values[q];
        }
    }
    auto basis_csc = SparseCsc::from_columns(m, basis_cols);
    try {
        (void)SparseLu::factorize(basis_csc, 1e-14);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    return basis;
}
}

}
