#include "sparse_basis_internal.hpp"
namespace markov_cero::linalg {
using namespace detail_sparse_basis;
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
}
