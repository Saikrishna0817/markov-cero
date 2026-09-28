#pragma once
#include "markov_cero/linalg/sparse_basis.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace markov_cero::linalg {
namespace detail_sparse_basis {}
namespace detail_sparse_basis {
constexpr std::size_t kMaxOrderingDimension = 2048;
}
namespace detail_sparse_basis {
constexpr double kMaxOrderingDensity = 0.2;
}
namespace detail_sparse_basis { void require_finite(double v, const char* message); }
namespace detail_sparse_basis { std::size_t count_nonzero(const std::vector<double>& v); }
namespace detail_sparse_basis { std::vector<std::size_t> minimum_degree_column_order(
    std::size_t n, const std::vector<std::vector<std::size_t>>& column_rows); }
namespace detail_sparse_basis { void validate_options(const SparseBasisOptions& o); }
double sparse_infinity_residual(const SparseCsc& matrix, const std::vector<double>& x,
                                const std::vector<double>& rhs, bool transpose);
double sparse_condition_estimate(const SparseLuDiagnostics& diagnostics);
}
