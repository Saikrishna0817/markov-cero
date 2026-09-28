#include "markov_cero/qp/model.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace markov_cero::qp {

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

ConvexityReport assess_convexity(const SparseSymmetricMatrix& P, double tolerance,
                                std::size_t maximum_factor_nonzeros,
                                std::optional<std::chrono::steady_clock::time_point> deadline) {
    ConvexityReport report;
    report.minimum_pivot = std::numeric_limits<double>::infinity();
    const std::size_t n = P.dimension;
    if (!std::isfinite(tolerance) || tolerance < 0.0 || maximum_factor_nonzeros == 0) {
        report.message = "invalid convexity-check tolerance or fill limit";
        return report;
    }
    try {
        P.validate();
    } catch (const std::exception& e) {
        report.message = std::string("invalid sparse symmetric matrix: ") + e.what();
        return report;
    }
    if (n == 0) {
        report.status = ConvexityStatus::positive_semidefinite;
        report.minimum_pivot = 0.0;
        report.message = "empty matrix is positive semidefinite";
        return report;
    }
    const auto deadline_expired = [&]() {
        return deadline && std::chrono::steady_clock::now() >= *deadline;
    };
    std::size_t work = 0;

    double matrix_scale = 1.0;
    for (double value : P.values) {
        if (!std::isfinite(value)) {
            report.message = "matrix contains a non-finite coefficient";
            return report;
        }
        matrix_scale = std::max(matrix_scale, std::abs(value));
    }
    const double pivot_tolerance = tolerance * matrix_scale;
    std::vector<double> diagonal(n, 0.0);
    std::vector<std::map<std::size_t, double>> adjacency(n);
    std::size_t stored_edges = 0;
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t k = P.column_offsets[j]; k < P.column_offsets[j + 1]; ++k) {
            if ((++work & 1023U) == 0U && deadline_expired()) {
                report.deadline_reached = true;
                report.message = "deadline reached while building sparse convexity matrix";
                return report;
            }
            const std::size_t i = P.row_indices[k];
            const double value = P.values[k];
            if (i == j) {
                diagonal[j] += value;
            } else {
                adjacency[i][j] += value;
                adjacency[j][i] += value;
                ++stored_edges;
            }
        }
    }
    report.factor_nonzeros += n;
    if (stored_edges + n > maximum_factor_nonzeros) {
        report.message = "input sparsity exceeds the convexity checker fill budget";
        return report;
    }

    // Minimum-degree pivot ordering controls sparse fill; negative diagonal
    // values remain direct witnesses of non-convexity.
    using Pivot = std::tuple<std::size_t, double, std::size_t, std::size_t>;
    std::priority_queue<Pivot, std::vector<Pivot>, std::greater<Pivot>> pivots;
    std::vector<std::size_t> versions(n, 0);
    std::vector<bool> is_active(n, true);
    for (std::size_t j = 0; j < n; ++j) {
        pivots.emplace(adjacency[j].size(), -diagonal[j], j, versions[j]);
    }
    std::size_t remaining = n;
    while (remaining > 0) {
        if (deadline_expired()) {
            report.deadline_reached = true;
            report.message = "deadline reached during sparse convexity factorization";
            return report;
        }
        while (!pivots.empty() &&
               (!is_active[std::get<2>(pivots.top())] ||
                std::get<3>(pivots.top()) != versions[std::get<2>(pivots.top())])) {
            pivots.pop();
        }
        if (pivots.empty()) {
            report.message = "sparse LDL pivot queue exhausted before completion";
            return report;
        }
        const std::size_t k = std::get<2>(pivots.top());
        const double pivot = -std::get<1>(pivots.top());
        pivots.pop();
        report.minimum_pivot = std::min(report.minimum_pivot, pivot);
        if (pivot < -pivot_tolerance) {
            report.status = ConvexityStatus::non_convex;
            std::ostringstream detail;
            detail.precision(17);
            detail << "negative LDL pivot " << pivot << " at variable " << k
                   << " certifies a non-convex direction";
            report.message = detail.str();
            return report;
        }

        if (pivot <= pivot_tolerance) {
            bool any_edge = false;
            double largest_edge = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                if (is_active[i] && i != k) {
                    const auto found = adjacency[i].find(k);
                    if (found != adjacency[i].end() && found->second != 0.0) {
                        any_edge = true;
                        largest_edge = std::max(largest_edge, std::abs(found->second));
                    }
                }
            }
            if (any_edge) {
                // A computed zero pivot with a nonzero Schur-complement edge
                // is an exact-arithmetic indefiniteness witness, but sparse
                // floating-point elimination can create tiny residual edges
                // for singular PSD matrices. Without a backward-error bound,
                // rejecting the input as non-convex would be unsound.
                std::ostringstream detail;
                detail.precision(17);
                detail << "near-zero diagonal " << pivot << " at variable " << k
                       << " with active edge " << largest_edge
                       << " is numerically indeterminate";
                report.message = detail.str();
                return report;
            }
            for (const auto& [i, value] : adjacency[k]) {
                (void)value;
                adjacency[i].erase(k);
                --stored_edges;
                ++versions[i];
                pivots.emplace(adjacency[i].size(), -diagonal[i], i, versions[i]);
            }
            is_active[k] = false;
            --remaining;
            adjacency[k].clear();
            continue;
        }

        std::vector<std::pair<std::size_t, double>> pivot_neighbors;
        pivot_neighbors.reserve(adjacency[k].size());
        for (const auto& [i, value] : adjacency[k]) {
            if (is_active[i] && value != 0.0) {
                pivot_neighbors.emplace_back(i, value);
            }
        }
        if (pivot_neighbors.size() > maximum_factor_nonzeros - report.factor_nonzeros) {
            report.message = "sparse LDL factor exceeded the convexity checker fill budget";
            return report;
        }
        report.factor_nonzeros += pivot_neighbors.size();

        if (pivot_neighbors.size() > 1) {
            const std::size_t m = pivot_neighbors.size();
            std::size_t new_fill = 0;
            for (std::size_t a = 0; a < m; ++a) {
                for (std::size_t b = a + 1; b < m; ++b) {
                    const auto i = pivot_neighbors[a].first;
                    const auto j = pivot_neighbors[b].first;
                    const auto found = adjacency[i].find(j);
                    if (found == adjacency[i].end() || found->second == 0.0) {
                        ++new_fill;
                    }
                }
            }
            if (new_fill > maximum_factor_nonzeros ||
                stored_edges > maximum_factor_nonzeros - new_fill) {
                report.message = "sparse LDL pivot would exceed the convexity checker fill budget";
                return report;
            }
        }
        std::vector<bool> changed(n, false);
        for (const auto& [i, aik] : pivot_neighbors) {
            diagonal[i] -= aik * aik / pivot;
            changed[i] = true;
        }
        for (std::size_t a = 0; a < pivot_neighbors.size(); ++a) {
            const auto [i, aik] = pivot_neighbors[a];
            for (std::size_t b = a + 1; b < pivot_neighbors.size(); ++b) {
                if ((++work & 1023U) == 0U && deadline_expired()) {
                    report.deadline_reached = true;
                    report.message = "deadline reached during sparse convexity factorization";
                    return report;
                }
                const auto [j, ajk] = pivot_neighbors[b];
                double target = 0.0;
                if (const auto found = adjacency[i].find(j); found != adjacency[i].end()) {
                    target = found->second;
                }
                const bool was_zero = target == 0.0;
                target -= aik * ajk / pivot;
                if (was_zero && target != 0.0) {
                    ++stored_edges;
                    if (stored_edges + n > maximum_factor_nonzeros) {
                        report.message = "sparse LDL fill exceeded the convexity checker budget";
                        return report;
                    }
                } else if (!was_zero && target == 0.0) {
                    --stored_edges;
                    adjacency[i].erase(j);
                    adjacency[j].erase(i);
                    changed[i] = changed[j] = true;
                    continue;
                } else if (was_zero && target == 0.0) {
                    continue;
                }
                if (was_zero) changed[i] = changed[j] = true;
                adjacency[i][j] = target;
                adjacency[j][i] = target;
            }
        }
        for (const auto& [i, value] : pivot_neighbors) {
            (void)value;
            if (adjacency[i].erase(k) > 0) {
                --stored_edges;
                changed[i] = true;
            }
        }
        adjacency[k].clear();
        is_active[k] = false;
        --remaining;
        for (const auto& [i, value] : pivot_neighbors) {
            (void)value;
            if (changed[i]) {
                ++versions[i];
                pivots.emplace(adjacency[i].size(), -diagonal[i], i, versions[i]);
            }
        }
    }

    report.status = ConvexityStatus::positive_semidefinite;
    if (!std::isfinite(report.minimum_pivot)) {
        report.minimum_pivot = 0.0;
    }
    report.message = "sparse LDL pivots are nonnegative within tolerance";
    return report;
}

bool check_convexity(const SparseSymmetricMatrix& P, double tolerance) {
    return assess_convexity(P, tolerance).status == ConvexityStatus::positive_semidefinite;
}

QuadraticModel make_quadratic_model(const model::Model& model) {
    QuadraticModel qp;
    qp.name = model.name;
    qp.sense = model.objective_sense;
    qp.objective_offset = model.objective_offset;

    const std::size_t n = model.matrix.column_count;
    const std::size_t m_orig = model.matrix.row_count;
    const double sign = (model.objective_sense == model::ObjectiveSense::maximize) ? -1.0 : 1.0;

    // Linear objective q
    qp.q.resize(n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
        if (j < model.objective.size()) {
            qp.q[j] = sign * model.objective[j];
        }
    }

    // Quadratic objective P (upper triangular)
    qp.P.dimension = n;
    qp.P.column_offsets.assign(n + 1, 0);
    if (model.has_quadratic_objective && model.quadratic_matrix.column_count == n) {
        std::vector<std::vector<std::pair<std::size_t, double>>> upper_entries(n);
        struct SymmetricPair {
            double upper{0.0};
            double lower{0.0};
            bool has_upper{false};
            bool has_lower{false};
        };
        std::map<std::pair<std::size_t, std::size_t>, SymmetricPair> symmetric_pairs;
        for (std::size_t j = 0; j < n; ++j) {
            const std::size_t start = model.quadratic_matrix.column_start[j];
            const std::size_t end = model.quadratic_matrix.column_start[j + 1];
            for (std::size_t k = start; k < end; ++k) {
                const std::size_t i = model.quadratic_matrix.row_index[k];
                const double v = sign * model.quadratic_matrix.value[k];
                auto& pair = symmetric_pairs[{std::min(i, j), std::max(i, j)}];
                if (i <= j) {
                    pair.upper += v;
                    pair.has_upper = true;
                } else {
                    pair.lower += v;
                    pair.has_lower = true;
                }
            }
        }
        // Parsed QUADOBJ matrices contain a mirrored pair; QMATRIX inputs may
        // already be full symmetric or provide one triangle. Convert either
        // representation to the single upper-triangle value expected by
        // SparseSymmetricMatrix. Averaging a pair also gives the mathematically
        // relevant symmetric part when a supplied quadratic matrix is slightly
        // asymmetric, since x^T Q x = x^T (Q + Q^T)/2 x.
        for (const auto& [indices, pair] : symmetric_pairs) {
            double value = 0.0;
            if (pair.has_upper && pair.has_lower) {
                value = 0.5 * (pair.upper + pair.lower);
            } else {
                value = pair.has_upper ? pair.upper : pair.lower;
            }
            upper_entries[indices.second].emplace_back(indices.first, value);
        }
        for (std::size_t j = 0; j < n; ++j) {
            std::sort(upper_entries[j].begin(), upper_entries[j].end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
            // Merge duplicate entries
            for (const auto& [r, v] : upper_entries[j]) {
                if (!qp.P.row_indices.empty() && qp.P.row_indices.back() == r &&
                    qp.P.column_offsets[j] < qp.P.row_indices.size()) {
                    qp.P.values.back() += v;
                } else {
                    qp.P.row_indices.push_back(r);
                    qp.P.values.push_back(v);
                }
            }
            qp.P.column_offsets[j + 1] = qp.P.values.size();
        }
    }

    // Combined constraints: m = m_orig + n (general constraints + variable box bounds)
    const std::size_t m_total = m_orig + n;
    qp.l.resize(m_total);
    qp.u.resize(m_total);

    for (std::size_t i = 0; i < m_orig; ++i) {
        qp.l[i] = (i < model.row_lower.size() && model.row_lower[i].is_finite())
                      ? model.row_lower[i].value
                      : -std::numeric_limits<double>::infinity();
        qp.u[i] = (i < model.row_upper.size() && model.row_upper[i].is_finite())
                      ? model.row_upper[i].value
                      : std::numeric_limits<double>::infinity();
    }
    for (std::size_t j = 0; j < n; ++j) {
        qp.l[m_orig + j] = (j < model.variable_lower.size() && model.variable_lower[j].is_finite())
                               ? model.variable_lower[j].value
                               : -std::numeric_limits<double>::infinity();
        qp.u[m_orig + j] = (j < model.variable_upper.size() && model.variable_upper[j].is_finite())
                               ? model.variable_upper[j].value
                               : std::numeric_limits<double>::infinity();
    }

    // Combined constraint matrix A: [A_orig; I_n]
    qp.A.rows = m_total;
    qp.A.columns = n;
    qp.A.column_offsets.assign(n + 1, 0);
    for (std::size_t j = 0; j < n; ++j) {
        const std::size_t start = (j < model.matrix.column_start.size())
                                      ? model.matrix.column_start[j]
                                      : 0;
        const std::size_t end = (j + 1 < model.matrix.column_start.size())
                                    ? model.matrix.column_start[j + 1]
                                    : start;
        for (std::size_t k = start; k < end; ++k) {
            qp.A.row_indices.push_back(model.matrix.row_index[k]);
            qp.A.values.push_back(model.matrix.value[k]);
        }
        // Identity entry at row m_orig + j
        qp.A.row_indices.push_back(m_orig + j);
        qp.A.values.push_back(1.0);
        qp.A.column_offsets[j + 1] = qp.A.values.size();
    }

    qp.variable_types = model.variable_type;
    qp.variable_names = model.variable_name;
    qp.constraint_names = model.row_name;
    for (std::size_t j = 0; j < n; ++j) {
        std::string vname = (j < model.variable_name.size() && !model.variable_name[j].empty())
                                ? model.variable_name[j]
                                : ("x" + std::to_string(j));
        qp.constraint_names.push_back("bnd_" + vname);
    }
    return qp;
}

} // namespace markov_cero::qp
