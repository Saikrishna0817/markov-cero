#include "model_internal.hpp"
namespace markov_cero::qp {
using namespace detail_model;
void SparseSymmetricMatrix::validate(std::size_t maximum_nonzeros) const {
    if (column_offsets.size() != dimension + 1) {
        throw std::invalid_argument("SparseSymmetricMatrix invalid column_offsets size");
    }
    if (column_offsets[0] != 0) {
        throw std::invalid_argument("SparseSymmetricMatrix column_offsets[0] must be 0");
    }
    if (row_indices.size() != values.size()) {
        throw std::invalid_argument("SparseSymmetricMatrix row/values size mismatch");
    }
    if (values.size() > maximum_nonzeros) {
        throw std::invalid_argument("SparseSymmetricMatrix nonzeros exceed maximum limit");
    }
    for (std::size_t j = 0; j < dimension; ++j) {
        const std::size_t start = column_offsets[j];
        const std::size_t end = column_offsets[j + 1];
        if (start > end || end > row_indices.size()) {
            throw std::invalid_argument("SparseSymmetricMatrix invalid column offset range");
        }
        std::size_t last_row = 0;
        for (std::size_t k = start; k < end; ++k) {
            const std::size_t r = row_indices[k];
            if (r > j) {
                throw std::invalid_argument(
                    "SparseSymmetricMatrix must be upper triangular (i<=j)");
            }
            if (k > start && r <= last_row) {
                throw std::invalid_argument("SparseSymmetricMatrix rows not strictly increasing");
            }
            last_row = r;
        }
    }
}
double SparseSymmetricMatrix::evaluate_energy(const std::vector<double>& x) const {
    if (x.size() != dimension) {
        throw std::invalid_argument("Vector size mismatch in evaluate_energy");
    }
    double sum = 0.0;
    for (std::size_t j = 0; j < dimension; ++j) {
        const double xj = x[j];
        const std::size_t start = column_offsets[j];
        const std::size_t end = column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            const std::size_t i = row_indices[k];
            const double v = values[k];
            if (i == j) {
                sum += v * xj * xj;
            } else {
                sum += 2.0 * v * x[i] * xj;
            }
        }
    }
    return sum;
}
std::vector<double> SparseSymmetricMatrix::multiply(const std::vector<double>& x) const {
    if (x.size() != dimension) {
        throw std::invalid_argument("Vector size mismatch in SparseSymmetricMatrix::multiply");
    }
    std::vector<double> y(dimension, 0.0);
    for (std::size_t j = 0; j < dimension; ++j) {
        const double xj = x[j];
        const std::size_t start = column_offsets[j];
        const std::size_t end = column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            const std::size_t i = row_indices[k];
            const double v = values[k];
            y[i] += v * xj;
            if (i != j) {
                y[j] += v * x[i];
            }
        }
    }
    return y;
}
linalg::SparseCsc SparseSymmetricMatrix::to_full_sparse_csc() const {
    std::vector<std::vector<std::pair<std::size_t, double>>> cols(dimension);
    for (std::size_t j = 0; j < dimension; ++j) {
        const std::size_t start = column_offsets[j];
        const std::size_t end = column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            const std::size_t i = row_indices[k];
            const double v = values[k];
            cols[j].emplace_back(i, v);
            if (i != j) {
                cols[i].emplace_back(j, v);
            }
        }
    }
    linalg::SparseCsc full;
    full.rows = dimension;
    full.columns = dimension;
    full.column_offsets.push_back(0);
    for (std::size_t j = 0; j < dimension; ++j) {
        std::sort(cols[j].begin(), cols[j].end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [r, v] : cols[j]) {
            full.row_indices.push_back(r);
            full.values.push_back(v);
        }
        full.column_offsets.push_back(full.values.size());
    }
    return full;
}
void QuadraticModel::validate() const {
    P.validate();
    A.validate();
    const std::size_t n = num_variables();
    const std::size_t m = num_constraints();
    if (P.dimension != n) {
        throw std::invalid_argument("QuadraticModel P dimension does not match variable count");
    }
    if (A.columns != n) {
        throw std::invalid_argument("QuadraticModel A columns do not match variable count");
    }
    if (A.rows != m || l.size() != m || u.size() != m) {
        throw std::invalid_argument("QuadraticModel constraint dimension mismatch");
    }
    for (std::size_t i = 0; i < m; ++i) {
        if (l[i] > u[i] + 1e-12) {
            throw std::invalid_argument(
                "QuadraticModel constraint lower bound exceeds upper bound");
        }
    }
}
}
