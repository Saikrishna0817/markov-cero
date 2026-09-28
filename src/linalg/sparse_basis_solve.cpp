#include "sparse_basis_internal.hpp"
namespace markov_cero::linalg {
using namespace detail_sparse_basis;
std::vector<double> SparseLu::solve_refined(const SparseCsc& matrix,
                                            const std::vector<double>& rhs,
                                            std::size_t max_steps,
                                            double early_exit_tol) const {
    auto x = solve(rhs);
    for (std::size_t step = 0; step < max_steps; ++step) {
        std::vector<long double> accum(matrix.rows, 0.0L);
        for (std::size_t j = 0; j < matrix.columns; ++j) {
            const long double xj = static_cast<long double>(x[j]);
            if (xj == 0.0L) continue;
            for (std::size_t p = matrix.column_offsets[j]; p < matrix.column_offsets[j + 1]; ++p) {
                accum[matrix.row_indices[p]] += static_cast<long double>(matrix.values[p]) * xj;
            }
        }
        std::vector<double> r(matrix.rows);
        double worst = 0.0;
        for (std::size_t i = 0; i < matrix.rows; ++i) {
            r[i] = static_cast<double>(static_cast<long double>(rhs[i]) - accum[i]);
            worst = std::max(worst, std::abs(r[i]));
        }
        if (worst < early_exit_tol)
            break;
        auto delta = solve(r);
        bool changed = false;
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double updated = x[i] + delta[i];
            if (updated != x[i] && std::isfinite(updated)) {
                x[i] = updated;
                changed = true;
            }
        }
        if (!changed)
            break;
    }
    return x;
}
std::vector<double> SparseLu::solve(const std::vector<double>& rhs) const {
    if (rhs.size() != dimension_)
        throw std::invalid_argument("sparse solve dimension mismatch");
    std::vector<double> y(dimension_);
    std::vector<bool> active(dimension_);
    for (std::size_t i = 0; i < dimension_; ++i) {
        require_finite(rhs[row_order_[i]], "non-finite sparse RHS");
        y[i] = rhs[row_order_[i]];
        active[i] = y[i] != 0;
    }
    for (std::size_t i = 0; i < dimension_; ++i)
        if (active[i]) {
            long double value = y[i];
            for (const auto& [j, a] : lower_rows_[i])
                value -= static_cast<long double>(a) * y[j];
            y[i] = static_cast<double>(value);
            require_finite(y[i], "non-finite sparse forward solve");
            if (y[i] != 0)
                for (const auto& [row, a] : lower_columns_[i]) {
                    (void)a;
                    active[row] = true;
                }
        }
    std::vector<double> x = y;
    std::fill(active.begin(), active.end(), false);
    for (std::size_t i = 0; i < dimension_; ++i)
        active[i] = x[i] != 0;
    for (std::size_t ii = dimension_; ii-- > 0;)
        if (active[ii]) {
            long double value = x[ii];
            double diagonal = 0;
            for (const auto& [j, a] : upper_rows_[ii]) {
                if (j == ii)
                    diagonal = a;
                else
                    value -= static_cast<long double>(a) * x[j];
            }
            if (diagonal == 0)
                throw std::runtime_error("zero sparse diagonal");
            x[ii] = static_cast<double>(value / diagonal);
            require_finite(x[ii], "non-finite sparse back solve");
            if (x[ii] != 0)
                for (const auto& [row, a] : upper_columns_[ii])
                    if (row < ii) {
                        (void)a;
                        active[row] = true;
                    }
        }
    // R6: map the position-space solution back to the input column order
    // (x = Q * z for the factorization P B Q = L U). The identity ordering makes
    // this a copy.
    std::vector<double> result(dimension_);
    for (std::size_t i = 0; i < dimension_; ++i)
        if (i < column_order_.size())
            result[column_order_[i]] = x[i];
    return result;
}
std::vector<double> SparseLu::solve_transpose(const std::vector<double>& rhs) const {
    if (rhs.size() != dimension_)
        throw std::invalid_argument("sparse transpose solve dimension mismatch");
    // R6: permute the RHS into position space (Q^T b) to match the ordered
    // factorization; the final row mapping below returns x in input row order.
    std::vector<double> y(dimension_);
    for (std::size_t i = 0; i < dimension_; ++i)
        y[i] = i < column_order_.size() ? rhs[column_order_[i]] : rhs[i];
    std::vector<bool> active(dimension_);
    for (std::size_t i = 0; i < dimension_; ++i) {
        require_finite(y[i], "non-finite sparse transpose RHS");
        active[i] = y[i] != 0;
    }
    for (std::size_t i = 0; i < dimension_; ++i)
        if (active[i]) {
            long double value = y[i];
            double diagonal = 0;
            for (const auto& [row, a] : upper_columns_[i]) {
                if (row == i)
                    diagonal = a;
                else if (row < i)
                    value -= static_cast<long double>(a) * y[row];
            }
            if (diagonal == 0)
                throw std::runtime_error("zero sparse transpose diagonal");
            y[i] = static_cast<double>(value / diagonal);
            require_finite(y[i], "non-finite sparse transpose U solve");
            if (y[i] != 0)
                for (const auto& [j, a] : upper_rows_[i])
                    if (j > i) {
                        (void)a;
                        active[j] = true;
                    }
        }
    std::vector<double> z = y;
    std::fill(active.begin(), active.end(), false);
    for (std::size_t i = 0; i < dimension_; ++i)
        active[i] = z[i] != 0;
    for (std::size_t ii = dimension_; ii-- > 0;)
        if (active[ii]) {
            long double value = z[ii];
            for (const auto& [row, a] : lower_columns_[ii])
                value -= static_cast<long double>(a) * z[row];
            z[ii] = static_cast<double>(value);
            require_finite(z[ii], "non-finite sparse transpose L solve");
            if (z[ii] != 0)
                for (const auto& [j, a] : lower_rows_[ii]) {
                    (void)a;
                    active[j] = true;
                }
        }
    std::vector<double> x(dimension_);
    for (std::size_t i = 0; i < dimension_; ++i)
        x[row_order_[i]] = z[i];
    return x;
}
}
