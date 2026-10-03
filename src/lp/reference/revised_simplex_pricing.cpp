#include "revised_simplex_internal.hpp"
namespace markov_cero::lp::reference {
using namespace detail_revised_simplex;
namespace detail_revised_simplex {
std::size_t checked_add(std::size_t a, std::size_t b) {
    if (b > std::numeric_limits<std::size_t>::max() - a)
        throw std::length_error("simplex dimension addition overflow");
    return a + b;
}
}

namespace detail_revised_simplex {
std::vector<double> column(const Work& w, std::size_t j) {
    std::vector<double> v(w.rows);
    for (const auto& [i, value] : w.a[j]) v[i] = value;
    return v;
}
}

namespace detail_revised_simplex {
double column_dot(const Work& w, std::size_t j, const std::vector<double>& y) {
    long double value = 0;
    for (const auto& [i, coefficient] : w.a[j]) value += static_cast<long double>(coefficient) * y[i];
    return static_cast<double>(value);
}
}

namespace detail_revised_simplex {
linalg::SparseBasisOptions sparse_options(const Options& o) {
    linalg::SparseBasisOptions so;
    // Basis acceptance floor: a fresh LU pivot at or below this is treated as
    // singular (the trial basis is rejected). Kept well above the raw
    // pivot_tolerance so bases whose solves only carry ~1e-10 relative accuracy
    // (accepted at 1e-12 but producing dual residuals of order 1, cf. scsd1's
    // warm-start phase II) never enter the pivot sequence in the first place.
    so.singular_tolerance = std::max(o.pivot_tolerance, 1e-10);
    so.update_pivot_tolerance = o.pivot_tolerance;
    so.maximum_dimension = maximum_rows;
    so.maximum_nonzeros = maximum_expanded_elements;
    so.maximum_factor_nonzeros = maximum_expanded_elements;
    so.maximum_updates = 16;
    so.eta_density_trigger = 0.9;
    so.deadline = o.deadline;
    return so;
}
}

namespace detail_revised_simplex {
linalg::SparseBasisFactorization make_factor(const Work& w,
                                             const linalg::SparseBasisOptions& opts) {
    linalg::SparseCsc matrix;
    matrix.rows = w.rows; matrix.columns = w.rows;
    matrix.column_offsets.push_back(0);
    for (auto j : w.basis) {
        for (const auto& [i, value] : w.a[j]) {
            matrix.row_indices.push_back(i); matrix.values.push_back(value);
        }
        matrix.column_offsets.push_back(matrix.values.size());
    }
    return linalg::SparseBasisFactorization::factorize(matrix, opts);
}
}

namespace detail_revised_simplex {
double dot(const std::vector<double>& a, const std::vector<double>& b) {
    long double s = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        s += static_cast<long double>(a[i]) * b[i];
    }
    const double d = static_cast<double>(s);
    if (!std::isfinite(d)) {
        throw std::overflow_error("non-finite simplex dot product");
    }
    return d;
}
}

namespace detail_revised_simplex {
void record_condition(linalg::SparseBasisFactorization& factor,
                             IterationOutcome& out) {
    out.condition_estimate = factor.current_condition_estimate();
}
}

namespace detail_revised_simplex {
bool significant_negative_reduced_cost(const Work& w, std::size_t j,
                                       const std::vector<double>& cost,
                                       const std::vector<double>& y, double rc, double tol) {
    long double scale = std::abs(static_cast<long double>(cost[j]));
    for (const auto& [i, value] : w.a[j]) {
        scale += std::abs(static_cast<long double>(value) * y[i]);
    }
    const double z = static_cast<double>(scale);
    const double allowed =
        tol * z + 64.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, z);
    return rc < -allowed;
}
}

namespace detail_revised_simplex {
void snap_basic_solution(std::vector<double>& xb, double feasibility_tolerance) {
    double max_norm = 1.0;
    for (double v : xb) {
        max_norm = std::max(max_norm, std::abs(v));
    }
    const double tol = std::max({feasibility_tolerance, 1e-6, 1e-8 * max_norm});
    for (double& v : xb) {
        if (v < 0 && v >= -tol) v = 0.0;
        if (v < 0) {

            throw std::runtime_error("primal basis lost feasibility");
        }
    }
}
}

namespace detail_revised_simplex {
std::size_t select_entering(const Work& w, const std::vector<double>& cost,
                                    const std::vector<double>& y, const std::vector<bool>& basic,
                                    std::size_t enter_limit, const Options& o, double& minimum_rc) {
    std::size_t entering = enter_limit;
    minimum_rc = 0;
    for (std::size_t j = 0; j < enter_limit; ++j) {
        if (basic[j]) {
            continue;
        }
        const double rc = cost[j] - column_dot(w, j, y);
        if (!significant_negative_reduced_cost(w, j, cost, y, rc, o.dual_tolerance)) {
            continue;
        }
        if (o.bland_anti_cycling) {
            entering = j;
            minimum_rc = rc;
            break;
        }
        if (entering == enter_limit || rc < minimum_rc) {
            entering = j;
            minimum_rc = rc;
        }
    }
    return entering;
}
}

namespace detail_revised_simplex {
std::size_t select_leaving(const Work& w, const std::vector<double>& xb,
                           const std::vector<double>& d, const Options& o, double& theta) {
    std::size_t leaving_row = w.rows;
    theta = std::numeric_limits<double>::infinity();
    // Choose a numerically stable pivot first, then check every tiny positive
    // coefficient against that step. A tiny row can still limit the step or
    // rule out an unbounded ray, even when it cannot form a stable basis.
    for (int pass = 0; pass < 2; ++pass) {
        const double stable_theta = theta;
        const bool has_stable = leaving_row < w.rows;
        for (std::size_t i = 0; i < w.rows; ++i) {
            if (d[i] <= 0.0 || (d[i] > o.pivot_tolerance) != (pass == 0)) continue;
            if (pass == 1 && has_stable &&
                static_cast<long double>(xb[i]) -
                    static_cast<long double>(d[i]) * stable_theta >=
                    -o.feasibility_tolerance) continue;
            const double ratio = xb[i] / d[i];
            if (!std::isfinite(ratio)) {
                if (has_stable) continue;
                throw std::overflow_error("non-finite simplex ratio");
            }
            if (ratio < theta ||
                (ratio == theta &&
                 (leaving_row == w.rows || w.basis[i] < w.basis[leaving_row]))) {
                theta = ratio;
                leaving_row = i;
            }
        }
    }
    return leaving_row;
}
}

}
