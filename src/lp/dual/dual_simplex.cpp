#include "dual_simplex_internal.hpp"
namespace markov_cero::lp::dual {
using namespace detail_dual_simplex;
namespace detail_dual_simplex {
std::uint64_t mix(std::uint64_t h, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        h ^= (v >> (8 * i)) & 255U;
        h *= 1099511628211ULL;
    }
    return h;
}
}

namespace detail_dual_simplex {
std::string hex(std::uint64_t h) {
    std::ostringstream o;
    o << std::hex << std::setw(16) << std::setfill('0') << h;
    return o.str();
}
}

namespace detail_dual_simplex {
std::uint64_t hash_text(const std::string& s) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}
}

namespace detail_dual_simplex {
void check_product(std::size_t a, std::size_t b) {
    if (a && b > std::numeric_limits<std::size_t>::max() / a) {
        throw std::length_error("dual simplex size overflow");
    }
    if (a * b > maximum_dense_elements) {
        throw std::length_error("dual simplex dense workspace limit exceeded");
    }
}
}

namespace detail_dual_simplex {
double dot(const std::vector<double>& a, const std::vector<double>& b) {
    long double s = 0;
    for (std::size_t i = 0; i < a.size(); ++i) s += static_cast<long double>(a[i]) * b[i];
    double v = static_cast<double>(s);
    if (!std::isfinite(v)) throw std::overflow_error("non-finite dual simplex dot product");
    return v;
}
}

namespace detail_dual_simplex {
linalg::SparseCsc sparse_basis_matrix(const transform::CanonicalModel& m,
                                      const std::vector<std::size_t>& basis) {
    std::vector<std::vector<double>> columns;
    columns.reserve(basis.size());
    for (auto j : basis) {
        std::vector<double> column(m.matrix.rows);
        for (std::size_t i = 0; i < m.matrix.rows; ++i) column[i] = m.matrix(i, j);
        columns.push_back(std::move(column));
    }
    return linalg::SparseCsc::from_columns(m.matrix.rows, columns);
}
}

namespace detail_dual_simplex {
linalg::SparseBasisOptions sparse_options(const Options& o) {
    linalg::SparseBasisOptions so;
    so.singular_tolerance = o.pivot_tolerance;
    so.update_pivot_tolerance = o.pivot_tolerance;
    so.maximum_dimension = maximum_rows;
    so.maximum_nonzeros = maximum_dense_elements;
    so.maximum_factor_nonzeros = maximum_dense_elements;
    so.maximum_updates = 16;
    so.eta_density_trigger = 0.9;
    so.deadline = o.deadline;
    return so;
}
}

namespace detail_dual_simplex {
bool significant_negative_reduced_cost(const transform::CanonicalModel& m, std::size_t j,
                                       const std::vector<double>& y, double rc, double tol) {
    long double scale = std::abs(static_cast<long double>(m.objective[j]));
    for (std::size_t i = 0; i < m.matrix.rows; ++i) {
        scale += std::abs(static_cast<long double>(m.matrix(i, j)) * y[i]);
    }
    const double z = static_cast<double>(scale);
    const double allowed =
        tol * z + 64.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, z);
    return rc < -allowed;
}
}

namespace detail_dual_simplex {
std::vector<double> full_primal(std::size_t n, const std::vector<std::size_t>& basis,
                                const std::vector<double>& xb) {
    std::vector<double> x(n);
    for (std::size_t i = 0; i < basis.size(); ++i) x[basis[i]] = xb[i];
    return x;
}
}

namespace detail_dual_simplex {
void validate_options(const Options& o) {
    if (!o.iteration_limit || o.iteration_limit > maximum_iterations ||
        o.telemetry_limit > maximum_telemetry || !std::isfinite(o.feasibility_tolerance) ||
        !std::isfinite(o.dual_tolerance) || !std::isfinite(o.pivot_tolerance) ||
        !std::isfinite(o.condition_trigger) || o.feasibility_tolerance <= 0 ||
        o.dual_tolerance <= 0 || o.pivot_tolerance <= 0 || o.condition_trigger < 0 ||
        o.feasibility_tolerance > maximum_tolerance || o.dual_tolerance > maximum_tolerance ||
        o.pivot_tolerance > maximum_tolerance || o.pivot_tolerance > o.feasibility_tolerance ||
        o.condition_trigger > 1) {
        throw std::invalid_argument("invalid dual simplex options");
    }
}
}

namespace detail_dual_simplex {
void validate_basis_metadata(const transform::CanonicalModel& m, const BasisState& s) {
    if (s.rows != m.matrix.rows || s.columns != m.matrix.columns ||
        s.model_fingerprint != fingerprint(m) || s.basic_variables.size() != m.matrix.rows) {
        throw std::invalid_argument("warm basis metadata mismatch");
    }
    std::vector<bool> seen(m.matrix.columns);
    for (auto j : s.basic_variables) {
        if (j >= m.matrix.columns || seen[j])
            throw std::invalid_argument("warm basis index invalid or duplicate");
        seen[j] = true;
    }
    if (m.matrix.rows > m.matrix.columns) {
        throw std::invalid_argument("warm basis cannot be square");
    }
}
}

namespace detail_dual_simplex {
void validate_basis(const transform::CanonicalModel& m, const BasisState& s) {
    validate_basis_metadata(m, s);
    try {
        (void)linalg::SparseLu::factorize(sparse_basis_matrix(m, s.basic_variables));
    } catch (const std::exception&) {
        throw std::invalid_argument("warm basis is singular");
    }
}
}

namespace detail_dual_simplex {
Result cold(const transform::CanonicalModel& m, const Options& o, const std::string& why) {
    Result out;
    reference::Options ro;
    ro.iteration_limit = o.iteration_limit;
    ro.telemetry_limit = o.telemetry_limit;
    ro.feasibility_tolerance = o.feasibility_tolerance;
    ro.dual_tolerance = o.dual_tolerance;
    ro.pivot_tolerance = o.pivot_tolerance;
    ro.deadline = o.deadline;
    auto r = reference::solve(m, ro);
    out.solution = std::move(r);
    out.used_cold_fallback = true;
    out.message = why;
    if (out.solution.status == reference::SolveStatus::optimal &&
        out.solution.basis.size() == m.matrix.rows) {
        try {
            out.basis_state = make_basis_state(m, out.solution.basis);
        } catch (...) {
            // Degenerate optimal basis: keep the solve result, drop the warm start.
        }
    }
    return out;
}
}

namespace detail_dual_simplex {
std::vector<double> compute_exact_dse_weights(const transform::CanonicalModel& m,
                                              linalg::SparseBasisFactorization& factor) {
    std::vector<double> gamma(m.matrix.rows, 1.0);
    for (std::size_t i = 0; i < m.matrix.rows; ++i) {
        std::vector<double> e(m.matrix.rows, 0.0);
        e[i] = 1.0;
        auto pi = factor.solve_transpose(e);
        gamma[i] = std::max(dot(pi, pi), 1e-12);
    }
    return gamma;
}
}

namespace detail_dual_simplex {
std::size_t select_leaving_row(const transform::CanonicalModel& m,
                               linalg::SparseBasisFactorization& factor,
                               const std::vector<double>& xb, const std::vector<std::size_t>& basis,
                               const std::vector<double>& dse_weights,
                               const Options& o, double& worst) {
    std::size_t leaving = m.matrix.rows;
    worst = 0;
    for (std::size_t i = 0; i < m.matrix.rows; ++i) {
        if (xb[i] >= -o.feasibility_tolerance) {
            continue;
        }
        if (o.pricing == PricingPolicy::bland) {
            if (leaving == m.matrix.rows || basis[i] < basis[leaving]) {
                leaving = i;
            }
            continue;
        }
        if (o.pricing == PricingPolicy::steepest_edge) {
            const double w = (i < dse_weights.size()) ? dse_weights[i] : 1.0;
            const double score =
                (-xb[i]) / std::sqrt(std::max(w, std::numeric_limits<double>::min()));
            if (leaving == m.matrix.rows || score > worst ||
                (score == worst && basis[i] < basis[leaving])) {
                leaving = i;
                worst = score;
            }
            continue;
        }
        // Tableau-norm weight ||A^T B^{-T} e_i||^2
        std::vector<double> e(m.matrix.rows);
        e[i] = 1;
        auto pi = factor.solve_transpose(e);
        auto row = linalg::multiply_transpose(m.matrix, pi);
        const double weight = dot(row, row);
        const double score =
            (-xb[i]) / std::sqrt(std::max(weight, std::numeric_limits<double>::min()));
        if (leaving == m.matrix.rows || score > worst ||
            (score == worst && basis[i] < basis[leaving])) {
            leaving = i;
            worst = score;
        }
    }
    return leaving;
}
}

// Repeated-solve session: keeps the accepted basis plus its factorization and
// exact steepest-edge weights across RHS-only / bound-only resolves. Anything
// that cannot certify a fresh optimal basis (infeasible, iteration limit,
// rejected verification, degenerate basis) drops both, so the next resolve is
// a cold solve.
Result Session::resolve(const transform::CanonicalModel& m, const Options& o) {
    ++resolve_count_;
    auto out = solve_verified(m, o, basis_, &cache_);
    last_verified_ = out.verified;
    if (out.verified) {
        ++verified_count_;
    }
    if (out.factor_reused) {
        ++factor_reuse_count_;
    }
    if (out.used_cold_fallback) {
        ++cold_fallback_count_;
    }
    if (out.verified && out.solution.status == reference::SolveStatus::optimal &&
        out.basis_state.basic_variables.size() == m.matrix.rows) {
        basis_ = out.basis_state;
    } else {
        basis_.reset();
        cache_.valid = false;
    }
    return out;
}

Session make_session(const Options& defaults) {
    return Session{defaults};
}

}
