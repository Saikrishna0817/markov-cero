#include "sparse_basis_internal.hpp"
namespace markov_cero::linalg {
using namespace detail_sparse_basis;
SparseLuSymbolicAnalysis SparseLu::analyze_sparsity(const SparseCsc& matrix, bool reduce_fill) {
    matrix.validate();
    if (matrix.rows != matrix.columns)
        throw std::invalid_argument("sparse LU requires square matrix");
    SparseLuSymbolicAnalysis out;
    out.dimension = matrix.rows;
    out.column_order.resize(matrix.columns);
    if (reduce_fill && matrix.columns > 0) {
        std::vector<std::vector<std::size_t>> column_rows(matrix.columns);
        for (std::size_t j = 0; j < matrix.columns; ++j)
            for (std::size_t p = matrix.column_offsets[j]; p < matrix.column_offsets[j + 1];
                 ++p)
                column_rows[j].push_back(matrix.row_indices[p]);
        out.column_order = minimum_degree_column_order(matrix.columns, column_rows);
    } else {
        for (std::size_t i = 0; i < out.column_order.size(); ++i)
            out.column_order[i] = i;
    }
    out.column_position.assign(matrix.columns, 0);
    for (std::size_t pos = 0; pos < out.column_order.size(); ++pos)
        out.column_position[out.column_order[pos]] = pos;
    return out;
}
SparseLu SparseLu::factorize_numeric(const SparseCsc& matrix,
                                     const SparseLuSymbolicAnalysis& symbolic,
                                     double singular_tolerance,
                                     std::size_t maximum_factor_nonzeros) {
    matrix.validate();
    require_finite(singular_tolerance, "non-finite sparse singular tolerance");
    if (singular_tolerance <= 0)
        throw std::invalid_argument("invalid sparse singular tolerance");
    if (matrix.rows != matrix.columns)
        throw std::invalid_argument("sparse LU requires square matrix");
    if (symbolic.dimension != matrix.rows || symbolic.column_order.size() != matrix.columns ||
        symbolic.column_position.size() != matrix.columns)
        throw std::invalid_argument("sparse LU symbolic dimension mismatch");

    SparseLu out;
    out.dimension_ = matrix.rows;
    out.row_order_.resize(matrix.rows);
    for (std::size_t i = 0; i < matrix.rows; ++i)
        out.row_order_[i] = i;
    out.column_order_ = symbolic.column_order;

    using Entry = std::pair<std::size_t, double>;
    std::vector<std::vector<Entry>> rows(matrix.rows);
    double maximum_original = 0;
    for (std::size_t j = 0; j < matrix.columns; ++j) {
        for (std::size_t p = matrix.column_offsets[j]; p < matrix.column_offsets[j + 1]; ++p) {
            rows[matrix.row_indices[p]].push_back({symbolic.column_position[j], matrix.values[p]});
            maximum_original = std::max(maximum_original, std::abs(matrix.values[p]));
        }
    }
    for (auto& r : rows) {
        std::sort(r.begin(), r.end(),
                  [](const Entry& a, const Entry& b) { return a.first < b.first; });
    }
    auto find_col = [](const std::vector<Entry>& r, std::size_t c) -> const Entry* {
        auto it = std::lower_bound(
            r.begin(), r.end(), c, [](const Entry& e, std::size_t col) { return e.first < col; });
        return (it != r.end() && it->first == c) ? &(*it) : nullptr;
    };
    auto find_col_mut = [](std::vector<Entry>& r, std::size_t c) -> Entry* {
        auto it = std::lower_bound(
            r.begin(), r.end(), c, [](const Entry& e, std::size_t col) { return e.first < col; });
        return (it != r.end() && it->first == c) ? &(*it) : nullptr;
    };
    out.diagnostics_.minimum_absolute_pivot =
        matrix.rows ? std::numeric_limits<double>::infinity() : 0;
    double maximum_factor = maximum_original;
    for (std::size_t k = 0; k < matrix.rows; ++k) {
        // Step 1: Find column maximum magnitude among rows i >= k
        double max_col_abs = 0;
        for (std::size_t i = k; i < matrix.rows; ++i) {
            const auto* entry = find_col(rows[i], k);
            if (entry && std::abs(entry->second) > max_col_abs) {
                max_col_abs = std::abs(entry->second);
            }
        }
        if (!std::isfinite(max_col_abs) || max_col_abs <= singular_tolerance)
            throw std::runtime_error("singular sparse basis");

        // Step 2: Threshold Markowitz search (u = 0.1, Markowitz 1957; Suhl & Suhl 1990)
        // Select pivot row i >= k satisfying |a_{ik}| >= 0.1 * max_col_abs that minimizes active row length.
        const double threshold = 0.1 * max_col_abs;
        std::size_t pivot_row = k;
        std::size_t min_row_nnz = std::numeric_limits<std::size_t>::max();
        double best_pivot_abs = 0;

        for (std::size_t i = k; i < matrix.rows; ++i) {
            const auto* entry = find_col(rows[i], k);
            if (!entry) continue;
            const double val_abs = std::abs(entry->second);
            if (val_abs >= threshold) {
                // Count active nonzeros in row i (entries with column index >= k)
                std::size_t active_nnz = 0;
                for (const auto& e : rows[i]) {
                    if (e.first >= k) ++active_nnz;
                }
                if (active_nnz < min_row_nnz || (active_nnz == min_row_nnz && val_abs > best_pivot_abs)) {
                    min_row_nnz = active_nnz;
                    pivot_row = i;
                    best_pivot_abs = val_abs;
                }
            }
        }

        if (pivot_row != k) {
            std::swap(rows[pivot_row], rows[k]);
            std::swap(out.row_order_[pivot_row], out.row_order_[k]);
        }
        const auto* pivot_entry = find_col(rows[k], k);
        if (!pivot_entry)
            throw std::runtime_error("singular sparse basis");
        const double pivot = pivot_entry->second;
        out.diagnostics_.minimum_absolute_pivot =
            std::min(out.diagnostics_.minimum_absolute_pivot, std::abs(pivot));
        out.diagnostics_.maximum_absolute_pivot =
            std::max(out.diagnostics_.maximum_absolute_pivot, std::abs(pivot));
        for (std::size_t i = k + 1; i < matrix.rows; ++i) {
            auto* found = find_col_mut(rows[i], k);
            if (!found)
                continue;
            const double multiplier = found->second / pivot;
            require_finite(multiplier, "non-finite sparse elimination multiplier");
            found->second = multiplier;
            std::vector<Entry> merged;
            merged.reserve(rows[i].size() + rows[k].size());
            auto it_i = rows[i].begin();
            while (it_i != rows[i].end() && it_i->first <= k) {
                merged.push_back(*it_i++);
            }
            auto it_k = rows[k].begin();
            while (it_k != rows[k].end() && it_k->first <= k) {
                ++it_k;
            }
            while (it_i != rows[i].end() && it_k != rows[k].end()) {
                if (it_i->first < it_k->first) {
                    merged.push_back(*it_i++);
                } else if (it_k->first < it_i->first) {
                    double next = -multiplier * it_k->second;
                    require_finite(next, "non-finite sparse elimination result");
                    if (next != 0) {
                        merged.push_back({it_k->first, next});
                        maximum_factor = std::max(maximum_factor, std::abs(next));
                    }
                    ++it_k;
                } else {
                    double next = it_i->second - multiplier * it_k->second;
                    require_finite(next, "non-finite sparse elimination result");
                    if (next != 0) {
                        merged.push_back({it_i->first, next});
                        maximum_factor = std::max(maximum_factor, std::abs(next));
                    }
                    ++it_i;
                    ++it_k;
                }
            }
            while (it_i != rows[i].end()) {
                merged.push_back(*it_i++);
            }
            while (it_k != rows[k].end()) {
                double next = -multiplier * it_k->second;
                require_finite(next, "non-finite sparse elimination result");
                if (next != 0) {
                    merged.push_back({it_k->first, next});
                    maximum_factor = std::max(maximum_factor, std::abs(next));
                }
                ++it_k;
            }
            rows[i] = std::move(merged);
        }
        std::size_t factor_count = 0;
        for (const auto& row : rows) {
            factor_count += row.size();
            if (factor_count > maximum_factor_nonzeros)
                throw std::length_error("sparse factor fill limit exceeded");
        }
    }
    out.lower_rows_.resize(matrix.rows);
    out.upper_rows_.resize(matrix.rows);
    out.lower_columns_.resize(matrix.rows);
    out.upper_columns_.resize(matrix.rows);
    for (std::size_t i = 0; i < matrix.rows; ++i)
        for (const auto& [j, value] : rows[i]) {
            if (j < i) {
                out.lower_rows_[i].push_back({j, value});
                out.lower_columns_[j].push_back({i, value});
                ++out.diagnostics_.lower_nonzeros;
            } else {
                out.upper_rows_[i].push_back({j, value});
                out.upper_columns_[j].push_back({i, value});
                ++out.diagnostics_.upper_nonzeros;
            }
        }
    out.diagnostics_.factor_nonzeros =
        out.diagnostics_.lower_nonzeros + out.diagnostics_.upper_nonzeros;
    out.diagnostics_.growth_factor = maximum_original == 0 ? 0 : maximum_factor / maximum_original;
    return out;
}
SparseLu SparseLu::factorize(const SparseCsc& matrix, double singular_tolerance,
                             std::size_t maximum_factor_nonzeros, bool reduce_fill) {
    const auto symbolic = analyze_sparsity(matrix, reduce_fill);
    return factorize_numeric(matrix, symbolic, singular_tolerance, maximum_factor_nonzeros);
}
}
