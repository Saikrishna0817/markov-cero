#pragma once

#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace markov_cero::qp {

/// Symmetric sparse matrix storage for quadratic objective P.
/// Stored in Compressed Sparse Column (CSC) format with only upper-triangular entries (i <= j).
struct SparseSymmetricMatrix {
    std::size_t dimension{};
    std::vector<std::size_t> column_offsets;
    std::vector<std::size_t> row_indices;
    std::vector<double> values;

    void validate(std::size_t maximum_nonzeros = 4U * 1024U * 1024U) const;

    /// Evaluates quadratic form x^T P x = sum_{i,j} P_{ij} x_i x_j
    [[nodiscard]] double evaluate_energy(const std::vector<double>& x) const;

    /// Computes y = P * x (multiplying full symmetric matrix using upper triangular entries)
    [[nodiscard]] std::vector<double> multiply(const std::vector<double>& x) const;

    /// Converts to full linalg::SparseCsc matrix
    [[nodiscard]] linalg::SparseCsc to_full_sparse_csc() const;
};

/// Canonical Quadratic Programming model in OSQP standard form:
///   minimize   (1/2) x^T P x + q^T x + objective_offset
///   subject to l <= A x <= u
struct QuadraticModel {
    std::string name;
    model::ObjectiveSense sense{model::ObjectiveSense::minimize};
    double objective_offset{0.0};

    /// Objective quadratic matrix P (dimension n x n, upper-triangular symmetric)
    SparseSymmetricMatrix P;

    /// Linear objective vector q (dimension n)
    std::vector<double> q;

    /// Linear constraint matrix A in CSC format (dimension m x n)
    linalg::SparseCsc A;

    /// Lower bounds on constraints l (dimension m, can include -infinity)
    std::vector<double> l;

    /// Upper bounds on constraints u (dimension m, can include +infinity)
    std::vector<double> u;

    /// Variable types (for MIQP: continuous, integer, binary)
    std::vector<model::VariableType> variable_types;

    /// Variable names (dimension n) and constraint names (dimension m)
    std::vector<std::string> variable_names;
    std::vector<std::string> constraint_names;

    [[nodiscard]] std::size_t num_variables() const noexcept { return q.size(); }
    [[nodiscard]] std::size_t num_constraints() const noexcept { return l.size(); }

    void validate() const;
};

enum class ConvexityStatus { positive_semidefinite, non_convex, indeterminate };

struct ConvexityReport {
    ConvexityStatus status{ConvexityStatus::indeterminate};
    double minimum_pivot{0.0};
    std::size_t factor_nonzeros{0};
    bool deadline_reached{false};
    std::string message;
};

/// Numerically classify the inertia of a sparse symmetric matrix using sparse
/// LDL^T elimination. Near-zero pivots are accepted only when their remaining
/// row is also zero within tolerance. If fill exceeds the safety cap or
/// numerical evidence is inconclusive, returns indeterminate (never labels an
/// unknown matrix non-convex).
[[nodiscard]] ConvexityReport assess_convexity(
    const SparseSymmetricMatrix& P, double tolerance = 1e-10,
    std::size_t maximum_factor_nonzeros = 5U * 1024U * 1024U,
    std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt);

/// Compatibility predicate: true only when positive semidefiniteness is
/// certified within tolerance. Use assess_convexity when the reason matters.
[[nodiscard]] bool check_convexity(const SparseSymmetricMatrix& P, double tolerance = 1e-10);

/// Converts a parsed model::Model with optional quadratic terms into a canonical QuadraticModel.
/// Combines linear constraints and variable box bounds into a unified l <= A x <= u system.
[[nodiscard]] QuadraticModel make_quadratic_model(const model::Model& model);

} // namespace markov_cero::qp
