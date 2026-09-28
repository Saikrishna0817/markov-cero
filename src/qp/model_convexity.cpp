#include "model_internal.hpp"
namespace markov_cero::qp {
using namespace detail_model;
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
}
