#include "sparse_canonicalize_internal.hpp"
namespace markov_cero::transform {
using namespace detail_sparse_canonicalize;
namespace detail_sparse_canonicalize {
void ensure_finite(double v, const char* message) {
    if (!std::isfinite(v)) {
        throw std::overflow_error(message);
    }
}
}

void SparseCanonicalModel::validate() const {
    if (matrix.rows != rhs.size() || matrix.columns != objective.size()) {
        throw std::invalid_argument("sparse canonical dimensions disagree");
    }
    if (record.structural_variables > matrix.columns) {
        throw std::invalid_argument("structural variable count exceeds canonical columns");
    }
    if (record.objective_sign != 1.0 && record.objective_sign != -1.0) {
        throw std::invalid_argument("objective sign must be plus or minus one");
    }
    ensure_finite(objective_offset, "canonical offset non-finite");
    matrix.validate();
    for (double v : rhs) {
        ensure_finite(v, "canonical rhs non-finite");
    }
    for (double v : objective) {
        ensure_finite(v, "canonical objective non-finite");
    }
    for (const auto& m : record.variables) {
        ensure_finite(m.offset, "non-finite variable-map offset");
        if (m.canonical_index.size() != m.multiplier.size()) {
            throw std::invalid_argument("variable-map dimension mismatch");
        }
        for (std::size_t q = 0; q < m.canonical_index.size(); ++q) {
            if (m.canonical_index[q] >= record.structural_variables ||
                m.canonical_index[q] >= matrix.columns) {
                throw std::invalid_argument("variable-map index out of range");
            }
            ensure_finite(m.multiplier[q], "non-finite variable-map multiplier");
        }
    }
}
std::vector<double> SparseCanonicalModel::multiply(const std::vector<double>& x) const {
    if (x.size() != matrix.columns) {
        throw std::invalid_argument("dimension mismatch in sparse canonical multiply");
    }
    std::vector<double> y(matrix.rows, 0.0);
    for (std::size_t j = 0; j < matrix.columns; ++j) {
        const double xj = x[j];
        if (xj == 0.0) {
            continue;
        }
        const std::size_t start = matrix.column_offsets[j];
        const std::size_t end = matrix.column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            y[matrix.row_indices[k]] += matrix.values[k] * xj;
        }
    }
    for (double v : y) {
        ensure_finite(v, "non-finite result in sparse canonical multiply");
    }
    return y;
}
std::vector<double> SparseCanonicalModel::multiply_transpose(const std::vector<double>& y) const {
    if (y.size() != matrix.rows) {
        throw std::invalid_argument("dimension mismatch in sparse canonical multiply_transpose");
    }
    std::vector<double> x(matrix.columns, 0.0);
    for (std::size_t j = 0; j < matrix.columns; ++j) {
        long double s = 0.0;
        const std::size_t start = matrix.column_offsets[j];
        const std::size_t end = matrix.column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            s += static_cast<long double>(matrix.values[k]) * y[matrix.row_indices[k]];
        }
        const double val = static_cast<double>(s);
        ensure_finite(val, "non-finite result in sparse canonical multiply_transpose");
        x[j] = val;
    }
    return x;
}
CanonicalModel SparseCanonicalModel::to_dense() const {
    // RW-5 (R12): the dense fast path covers small/medium models; anything above
    // it needs a genuinely sparse LP solve path (tracked as P1 work, since the
    // reference/dual simplex workspaces are dense by design today). The error is
    // explicit and typed — never a silent wrong answer.
    constexpr std::size_t max_dense_rows = 4096;   // dual/reference row cap
    constexpr std::size_t max_dense_cols = 16384;
    if (matrix.rows > max_dense_rows || matrix.columns > max_dense_cols) {
        throw std::length_error("sparse model exceeds dense dimension limits "
                                "(rows<=4096, cols<=16384); sparse LP solve path "
                                "not yet implemented (P1)");
    }
    CanonicalModel d;
    d.matrix.rows = matrix.rows;
    d.matrix.columns = matrix.columns;
    d.matrix.values.assign(matrix.rows * matrix.columns, 0.0);
    for (std::size_t j = 0; j < matrix.columns; ++j) {
        const std::size_t start = matrix.column_offsets[j];
        const std::size_t end = matrix.column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            d.matrix(matrix.row_indices[k], j) += matrix.values[k];
        }
    }
    d.rhs = rhs;
    d.objective = objective;
    d.objective_offset = objective_offset;
    d.record = record;
    d.validate();
    return d;
}
}
