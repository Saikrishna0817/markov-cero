#include "markov_cero/linalg/sparse_basis.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace markov_cero::linalg {
namespace {
void require_finite(double v, const char* message) {
    if (!std::isfinite(v))
        throw std::invalid_argument(message);
}
std::size_t count_nonzero(const std::vector<double>& v) {
    return static_cast<std::size_t>(
        std::count_if(v.begin(), v.end(), [](double x) { return x != 0; }));
}
/// R6: static fill-reducing column ordering. Greedy minimum degree on the
/// column intersection graph (columns adjacent iff they share a row) — the
/// classical pre-ordering behind Markowitz/AMD/COLAMD-style sparse LU, applied
/// here as a permutation of the elimination order.
///
/// Returns `position -> original column`. Falls back to the identity when the
/// ordering cannot pay for itself: tiny matrices, very dense patterns (where the
/// graph is near-complete and no ordering helps), or dimensions above the
/// adjacency-matrix guard. The heuristics are exactly the "ordering quality"
/// concern of the sparse-LU literature (Markowitz 1957; Amestoy/Davis AMD):
/// ordering does not change the answer, only the fill it costs to get there.
constexpr std::size_t kMaxOrderingDimension = 2048;
constexpr double kMaxOrderingDensity = 0.2;

[[nodiscard]] std::vector<std::size_t> minimum_degree_column_order(
    std::size_t n, const std::vector<std::vector<std::size_t>>& column_rows) {
    std::vector<std::size_t> identity(n);
    for (std::size_t i = 0; i < n; ++i)
        identity[i] = i;
    if (n < 32 || n > kMaxOrderingDimension)
        return identity;

    std::size_t edges = 0;
    for (const auto& rows : column_rows)
        edges += rows.size();
    if (static_cast<double>(edges) > kMaxOrderingDensity * static_cast<double>(n) * n)
        return identity;  // dense basis: fill is unavoidable, ordering is pure cost

    // Dense adjacency matrix (n <= 2048 keeps this <= 4 MB) with degree counters;
    // the greedy pick is O(n) and each elimination merges the neighbourhood.
    std::vector<std::vector<char>> adjacent(n, std::vector<char>(n, 0));
    std::vector<std::size_t> degree(n, 0);
    // Build adjacency by bucketing columns per row (intersection graph).
    std::vector<std::vector<std::size_t>> row_columns;
    std::size_t max_row = 0;
    for (const auto& rows : column_rows)
        for (std::size_t r : rows)
            max_row = std::max(max_row, r);
    row_columns.assign(max_row + 1, {});
    for (std::size_t c = 0; c < n; ++c)
        for (std::size_t r : column_rows[c])
            row_columns[r].push_back(c);
    for (const auto& cols : row_columns)
        for (std::size_t a = 0; a < cols.size(); ++a)
            for (std::size_t b = a + 1; b < cols.size(); ++b) {
                const std::size_t u = cols[a];
                const std::size_t v = cols[b];
                if (!adjacent[u][v]) {
                    adjacent[u][v] = 1;
                    adjacent[v][u] = 1;
                    ++degree[u];
                    ++degree[v];
                }
            }

    constexpr std::size_t kEliminated = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> order;
    order.reserve(n);
    for (std::size_t step = 0; step < n; ++step) {
        std::size_t pick = n;
        for (std::size_t c = 0; c < n; ++c) {
            if (degree[c] == kEliminated)
                continue;
            if (pick == n || degree[c] < degree[pick])
                pick = c;
        }
        if (pick == n)
            return identity;  // defensive: should not happen
        order.push_back(pick);
        // Quotient-graph update: fill edges between the neighbours of `pick`,
        // then detach the column from its neighbours.
        std::vector<std::size_t> neighbours;
        for (std::size_t v = 0; v < n; ++v)
            if (adjacent[pick][v])
                neighbours.push_back(v);
        for (std::size_t a = 0; a < neighbours.size(); ++a)
            for (std::size_t b = a + 1; b < neighbours.size(); ++b) {
                const std::size_t u = neighbours[a];
                const std::size_t v = neighbours[b];
                if (!adjacent[u][v]) {
                    adjacent[u][v] = 1;
                    adjacent[v][u] = 1;
                    ++degree[u];
                    ++degree[v];
                }
            }
        for (std::size_t v : neighbours) {
            adjacent[pick][v] = 0;
            adjacent[v][pick] = 0;
            --degree[v];
        }
        degree[pick] = kEliminated;
    }
    return order;
}

void validate_options(const SparseBasisOptions& o) {
    require_finite(o.singular_tolerance, "non-finite sparse singular tolerance");
    require_finite(o.update_pivot_tolerance, "non-finite sparse update tolerance");
    require_finite(o.eta_density_trigger, "non-finite eta density trigger");
    if (o.singular_tolerance <= 0 || o.update_pivot_tolerance <= 0 || o.eta_density_trigger <= 0 ||
        o.eta_density_trigger > 1 || !o.maximum_updates || !o.maximum_dimension ||
        !o.maximum_nonzeros || !o.maximum_factor_nonzeros)
        throw std::invalid_argument("invalid sparse basis options");
    require_finite(o.refinement_trigger_growth, "non-finite refinement growth trigger");
    require_finite(o.refinement_trigger_condition, "non-finite refinement condition trigger");
    if (o.refinement_trigger_growth <= 0 || o.refinement_trigger_condition <= 0)
        throw std::invalid_argument("invalid sparse basis options");
}
} // namespace
void SparseCsc::validate(std::size_t maximum_nonzeros) const {
    // RW-5 (R12): the sparse path must not cap below the industrial scale the PS
    // requires ("thousands to millions"). The old 4096 hard cap rejected 10k-row
    // models at parse time. Dimensions are now bounded only by checked arithmetic
    // and the nonzero budget; per-solver caps (if any) live above this layer.
    // Degenerate dimensions are legal (a canonical model can have every
    // constraint absorbed by presolve; the audit tests exercise 0-row LPs).
    // Only unrepresentably large dimensions are rejected.
    if (rows > (std::size_t{1} << 40) || columns > (std::size_t{1} << 40))
        throw std::length_error("sparse matrix dimension limit exceeded");
    if (column_offsets.size() != columns + 1 || column_offsets.empty() ||
        column_offsets.front() != 0 || column_offsets.back() != values.size() ||
        row_indices.size() != values.size())
        throw std::invalid_argument("invalid CSC dimensions");
    if (values.size() > maximum_nonzeros)
        throw std::length_error("sparse matrix nonzero limit exceeded");
    for (std::size_t j = 0; j < columns; ++j) {
        if (column_offsets[j] > column_offsets[j + 1])
            throw std::invalid_argument("CSC offsets are not monotone");
        std::size_t previous = 0;
        bool first = true;
        for (std::size_t p = column_offsets[j]; p < column_offsets[j + 1]; ++p) {
            if (row_indices[p] >= rows || (!first && row_indices[p] <= previous))
                throw std::invalid_argument("CSC row indices are not canonical");
            require_finite(values[p], "non-finite sparse coefficient");
            if (values[p] == 0)
                throw std::invalid_argument("explicit zero in canonical CSC");
            previous = row_indices[p];
            first = false;
        }
    }
}
std::vector<double> SparseCsc::dense_column(std::size_t column) const {
    validate();
    if (column >= columns)
        throw std::out_of_range("sparse column out of range");
    std::vector<double> out(rows);
    for (std::size_t p = column_offsets[column]; p < column_offsets[column + 1]; ++p)
        out[row_indices[p]] = values[p];
    return out;
}
SparseCsc SparseCsc::from_columns(std::size_t row_count,
                                  const std::vector<std::vector<double>>& columns_data) {
    SparseCsc out;
    out.rows = row_count;
    out.columns = columns_data.size();
    out.column_offsets.push_back(0);
    for (const auto& column : columns_data) {
        if (column.size() != row_count)
            throw std::invalid_argument("sparse column dimension mismatch");
        for (std::size_t i = 0; i < row_count; ++i) {
            require_finite(column[i], "non-finite sparse column value");
            if (column[i] != 0) {
                out.row_indices.push_back(i);
                out.values.push_back(column[i]);
            }
        }
        out.column_offsets.push_back(out.values.size());
    }
    out.validate();
    return out;
}
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
SparseBasisFactorization SparseBasisFactorization::factorize(const SparseCsc& basis,
                                                             const SparseBasisOptions& options) {
    validate_options(options);
    basis.validate(options.maximum_nonzeros);
    if (basis.rows != basis.columns || basis.rows > options.maximum_dimension)
        throw std::invalid_argument("invalid sparse basis dimensions");
    SparseBasisFactorization out;
    out.options_ = options;
    out.current_basis_ = basis;
    out.base_ = SparseLu::factorize(basis, options.singular_tolerance,
                                    options.maximum_factor_nonzeros,
                                    options.fill_reducing_ordering);
    out.statistics_.refactorizations = 1;
    return out;
}
void SparseBasisFactorization::apply_updates(std::vector<double>& x) const {
    for (const auto& eta : updates_) {
        const double xp = x[eta.pivot] / eta.pivot_value;
        require_finite(xp, "non-finite eta solve pivot");
        for (const auto& [i, value] : eta.entries)
            if (i != eta.pivot)
                x[i] -= value * xp;
        x[eta.pivot] = xp;
    }
}
void SparseBasisFactorization::apply_updates_transpose(std::vector<double>& x) const {
    for (auto it = updates_.rbegin(); it != updates_.rend(); ++it) {
        long double value = x[it->pivot];
        for (const auto& [i, a] : it->entries)
            if (i != it->pivot)
                value -= static_cast<long double>(a) * x[i];
        x[it->pivot] = static_cast<double>(value / it->pivot_value);
        require_finite(x[it->pivot], "non-finite eta transpose solve");
    }
}
std::vector<double> SparseBasisFactorization::residual_vector(const std::vector<double>& rhs,
                                                              const std::vector<double>& x,
                                                              bool transpose) const {
    // Accumulate in long double: residual must be more accurate than the solve
    // itself or refinement feeds on noise (Skeel 1980).
    const std::size_t n = current_basis_.rows;
    std::vector<long double> product(n, 0.0L);
    if (!transpose)
        for (std::size_t j = 0; j < current_basis_.columns; ++j)
            for (std::size_t p = current_basis_.column_offsets[j];
                 p < current_basis_.column_offsets[j + 1]; ++p)
                product[current_basis_.row_indices[p]] +=
                    static_cast<long double>(current_basis_.values[p]) * x[j];
    else
        for (std::size_t j = 0; j < current_basis_.columns; ++j)
            for (std::size_t p = current_basis_.column_offsets[j];
                 p < current_basis_.column_offsets[j + 1]; ++p)
                product[j] += static_cast<long double>(current_basis_.values[p]) *
                              x[current_basis_.row_indices[p]];
    std::vector<double> r(n);
    for (std::size_t i = 0; i < n; ++i)
        r[i] = static_cast<double>(static_cast<long double>(rhs[i]) - product[i]);
    return r;
}
bool SparseBasisFactorization::refinement_required() const noexcept {
    return options_.maximum_refinement_steps > 0;
}
std::vector<double> SparseBasisFactorization::refine(std::vector<double> x,
                                                     const std::vector<double>& rhs,
                                                     bool transpose) {
    for (std::size_t step = 0; step < options_.maximum_refinement_steps; ++step) {
        const std::vector<double> r = residual_vector(rhs, x, transpose);
        double worst = 0.0;
        for (double v : r)
            worst = std::max(worst, std::abs(v));
        if (worst < 1e-14)
            break;  // early exit when residual is already below noise threshold
        ++statistics_.refinement_attempts;
        std::vector<double> correction;
        if (transpose) {
            correction = base_.solve_transpose(r);
            apply_updates_transpose(correction);
        } else {
            correction = base_.solve(r);
            apply_updates(correction);
        }
        bool changed = false;
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double updated = x[i] + correction[i];
            if (updated != x[i] && std::isfinite(updated)) {
                x[i] = updated;
                changed = true;
            }
        }
        if (!changed)
            break;
        ++statistics_.refinements_applied;
    }
    return x;
}
std::vector<double> SparseBasisFactorization::solve(const std::vector<double>& rhs) {
    statistics_.last_rhs_nonzeros = count_nonzero(rhs);
    auto x = base_.solve(rhs);
    apply_updates(x);
    for (double v : x)
        require_finite(v, "non-finite eta solve result");
    if (options_.maximum_refinement_steps > 0)
        x = refine(std::move(x), rhs, false);
    statistics_.last_solution_nonzeros = count_nonzero(x);
    return x;
}
std::vector<double> SparseBasisFactorization::solve_transpose(const std::vector<double>& rhs) {
    statistics_.last_rhs_nonzeros = count_nonzero(rhs);
    if (rhs.size() != current_basis_.rows)
        throw std::invalid_argument("eta transpose dimension mismatch");
    std::vector<double> work = rhs;
    apply_updates_transpose(work);
    auto x = base_.solve_transpose(work);
    if (options_.maximum_refinement_steps > 0)
        x = refine(std::move(x), rhs, true);
    statistics_.last_solution_nonzeros = count_nonzero(x);
    return x;
}
void SparseBasisFactorization::replace_column(std::size_t position,
                                              const std::vector<double>& column) {
    if (position >= current_basis_.columns || column.size() != current_basis_.rows)
        throw std::invalid_argument("sparse basis update dimension mismatch");
    for (double v : column)
        require_finite(v, "non-finite sparse update column");
    if (needs_refactorization())
        refactorize();
    SparseCsc next;
    next.rows = current_basis_.rows;
    next.columns = current_basis_.columns;
    next.column_offsets.push_back(0);
    for (std::size_t j = 0; j < current_basis_.columns; ++j) {
        if (j == position) {
            for (std::size_t i = 0; i < column.size(); ++i)
                if (column[i] != 0) {
                    next.row_indices.push_back(i);
                    next.values.push_back(column[i]);
                }
        } else
            for (std::size_t q = current_basis_.column_offsets[j];
                 q < current_basis_.column_offsets[j + 1]; ++q) {
                next.row_indices.push_back(current_basis_.row_indices[q]);
                next.values.push_back(current_basis_.values[q]);
            }
        next.column_offsets.push_back(next.values.size());
    }
    next.validate(options_.maximum_nonzeros);
    auto direction = solve(column);
    if (std::abs(direction[position]) <= options_.update_pivot_tolerance)
        throw std::runtime_error("unstable sparse basis update pivot");
    Eta eta;
    eta.pivot = position;
    eta.pivot_value = direction[position];
    for (std::size_t i = 0; i < direction.size(); ++i)
        if (direction[i] != 0)
            eta.entries.push_back({i, direction[i]});
    auto nnz = eta.entries.size();
    updates_.push_back(std::move(eta));
    current_basis_ = std::move(next);
    ++statistics_.updates;
    statistics_.current_update_chain = updates_.size();
    statistics_.maximum_eta_nonzeros = std::max(statistics_.maximum_eta_nonzeros, nnz);
    statistics_.update_limit_triggered = updates_.size() >= options_.maximum_updates;
    statistics_.density_triggered =
        !direction.empty() &&
        static_cast<double>(nnz) / direction.size() > options_.eta_density_trigger;
}
bool SparseBasisFactorization::needs_refactorization() const noexcept {
    return statistics_.update_limit_triggered || statistics_.density_triggered;
}
void SparseBasisFactorization::refactorize() {
    base_ = SparseLu::factorize(current_basis_, options_.singular_tolerance,
                                options_.maximum_factor_nonzeros);
    updates_.clear();
    ++statistics_.refactorizations;
    statistics_.current_update_chain = 0;
    statistics_.update_limit_triggered = false;
    statistics_.density_triggered = false;
}
double SparseBasisFactorization::current_condition_estimate() {
    if (current_basis_.rows == 0 || current_basis_.columns == 0) {
        // Empty system: conventionally perfectly conditioned; a 0-dim LU has
        // no pivots and would otherwise report a degenerate inf/0.
        return 1.0;
    }
    if (!updates_.empty()) {
        // Pending eta updates make base_ an LU of an OLDER basis; recompute so
        // the proxy describes the basis that produced the current answer.
        try {
            refactorize();
        } catch (const std::exception&) {
            // The current basis will not refactorize under the solve
            // tolerances: keep the cached base-LU diagnostics (the last
            // factorization that WAS valid) rather than reporting nothing.
        }
    }
    return sparse_condition_estimate(diagnostics());
}
double sparse_infinity_residual(const SparseCsc& matrix, const std::vector<double>& x,
                                const std::vector<double>& rhs, bool transpose) {
    matrix.validate();
    const std::size_t expected_x = transpose ? matrix.rows : matrix.columns;
    const std::size_t expected_rhs = transpose ? matrix.columns : matrix.rows;
    if (x.size() != expected_x || rhs.size() != expected_rhs)
        throw std::invalid_argument("sparse residual dimension mismatch");
    std::vector<long double> product(expected_rhs);
    if (!transpose) {
        for (std::size_t j = 0; j < matrix.columns; ++j) {
            require_finite(x[j], "non-finite sparse residual input");
            for (std::size_t p = matrix.column_offsets[j]; p < matrix.column_offsets[j + 1]; ++p)
                product[matrix.row_indices[p]] += static_cast<long double>(matrix.values[p]) * x[j];
        }
    } else
        for (std::size_t j = 0; j < matrix.columns; ++j)
            for (std::size_t p = matrix.column_offsets[j]; p < matrix.column_offsets[j + 1]; ++p) {
                require_finite(x[matrix.row_indices[p]], "non-finite sparse residual input");
                product[j] += static_cast<long double>(matrix.values[p]) * x[matrix.row_indices[p]];
            }
    double result = 0;
    for (std::size_t i = 0; i < rhs.size(); ++i) {
        require_finite(rhs[i], "non-finite sparse residual RHS");
        double residual = static_cast<double>(static_cast<long double>(rhs[i]) - product[i]);
        require_finite(residual, "non-finite sparse residual");
        result = std::max(result, std::abs(residual));
    }
    return result;
}
double sparse_condition_estimate(const SparseLuDiagnostics& diagnostics) {
    const double minimum = diagnostics.minimum_absolute_pivot;
    const double maximum = diagnostics.maximum_absolute_pivot;
    if (minimum <= 0 || maximum <= 0)
        return std::numeric_limits<double>::infinity();
    return maximum / minimum;
}
} // namespace markov_cero::linalg
