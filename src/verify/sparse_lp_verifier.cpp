#include "markov_cero/verify/reference_lp_verifier.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace markov_cero::verify {
namespace {
constexpr double roundoff_factor = 512.0 * std::numeric_limits<double>::epsilon();
bool finite(const std::vector<double>& v) {
    return std::all_of(v.begin(), v.end(), [](double x) { return std::isfinite(x); });
}
double dot(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size())
        throw std::invalid_argument("verification dot dimension mismatch");
    long double s = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        s += static_cast<long double>(a[i]) * b[i];
    const double v = static_cast<double>(s);
    if (!std::isfinite(v))
        throw std::overflow_error("verification dot overflow");
    return v;
}
double allowed(double scale, double tol) {
    if (!std::isfinite(scale) || scale < 0)
        throw std::overflow_error("verification scale invalid");
    const double v = (tol + roundoff_factor) * std::max(1.0, scale);
    if (!std::isfinite(v))
        throw std::overflow_error("verification allowance overflow");
    return v;
}
double dual_allowed(double scale, double tol) {
    // A tiny negative reduced cost can permit an arbitrarily large improvement
    // on an unbounded variable. Do not give it the primal absolute tolerance.
    if (!std::isfinite(scale) || scale < 0) throw std::overflow_error("invalid dual scale");
    return tol * scale + roundoff_factor * std::max(1.0, scale);
}
std::vector<double> row_scales(const linalg::SparseCsc& a,
                               const std::vector<double>& x,
                               const std::vector<double>& rhs) {
    std::vector<long double> sums(a.rows, 0.0L);
    for (std::size_t i = 0; i < rhs.size(); ++i) sums[i] = std::abs(rhs[i]);
    for (std::size_t j = 0; j < a.columns; ++j)
        for (std::size_t k = a.column_offsets[j]; k < a.column_offsets[j + 1]; ++k)
            sums[a.row_indices[k]] += std::abs(static_cast<long double>(a.values[k]) * x[j]);
    return {sums.begin(), sums.end()};
}
double col_scale(const linalg::SparseCsc& a, std::size_t j,
                 const std::vector<double>& y, double cost = 0) {
    long double s = std::abs(static_cast<long double>(cost));
    for (std::size_t k = a.column_offsets[j]; k < a.column_offsets[j + 1]; ++k)
        s += std::abs(static_cast<long double>(a.values[k]) * y[a.row_indices[k]]);
    return static_cast<double>(s);
}
} // namespace
ReferenceVerification verify_sparse_result(const transform::SparseCanonicalModel& m,
                                              const lp::reference::Result& r, double tol) {
    ReferenceVerification v;
    try {
        m.validate();
        if (!std::isfinite(tol) || tol <= 0 || tol > 1e-4) {
            v.message = "invalid verification tolerance";
            return v;
        }
        if (r.status == lp::reference::SolveStatus::optimal) {
            if (r.primal.size() != m.matrix.columns || r.dual.size() != m.matrix.rows ||
                !finite(r.primal) || !finite(r.dual) || !std::isfinite(r.objective)) {
                v.message = "invalid optimal payload";
                return v;
            }
            auto ax = m.multiply(r.primal);
            const auto primal_scale = row_scales(m.matrix, r.primal, m.rhs);
            bool pok = true;
            for (std::size_t i = 0; i < ax.size(); ++i) {
                double e = std::abs(ax[i] - m.rhs[i]);
                v.maximum_primal_violation = std::max(v.maximum_primal_violation, e);
                pok = pok && e <= allowed(primal_scale[i], tol);
            }
            for (double x : r.primal) {
                double e = std::max(0.0, -x);
                v.maximum_primal_violation = std::max(v.maximum_primal_violation, e);
                pok = pok && e <= allowed(std::abs(x), tol);
            }
            auto aty = m.multiply_transpose(r.dual);
            bool dok = true, cok = true;
            for (std::size_t j = 0; j < aty.size(); ++j) {
                const double c_scale = col_scale(m.matrix, j, r.dual, m.objective[j]);
                double rc = m.objective[j] - aty[j], e = std::max(0.0, -rc);
                v.maximum_dual_violation = std::max(v.maximum_dual_violation, e);
                dok = dok && e <= dual_allowed(c_scale, tol);
                double comp = std::abs(r.primal[j] * rc);
                v.maximum_complementarity_violation =
                    std::max(v.maximum_complementarity_violation, comp);
                const double comp_scale =
                    std::max(1.0, std::abs(r.primal[j])) * std::max(1.0, c_scale);
                cok = cok && comp <= allowed(comp_scale, tol);
            }
            double po = dot(m.objective, r.primal) + m.objective_offset,
                   du = dot(m.rhs, r.dual) + m.objective_offset,
                   scale = std::max({1.0, std::abs(po), std::abs(du), std::abs(r.objective)});
            bool ook = std::abs(po - r.objective) <= allowed(scale, tol) &&
                       std::abs(po - du) <= allowed(scale, tol);
            v.accepted = pok && dok && cok && ook;
            if (v.accepted) {
                v.message = "scaled optimality conditions verified";
            } else {
                v.message = std::string("scaled optimality verification failed:") +
                    (pok ? "" : " pok_fail") + (dok ? "" : " dok_fail") +
                    (cok ? "" : " cok_fail") + (ook ? "" : " ook_fail");
            }
            return v;
        }
        if (r.status == lp::reference::SolveStatus::infeasible) {
            if (r.certificate.size() != m.matrix.rows || !finite(r.certificate)) {
                v.message = "invalid infeasibility certificate";
                return v;
            }
            auto aty = m.multiply_transpose(r.certificate);
            bool ok = true;
            for (std::size_t j = 0; j < aty.size(); ++j) {
                double e = std::max(0.0, aty[j]);
                v.maximum_dual_violation = std::max(v.maximum_dual_violation, e);
                ok = ok && e <= dual_allowed(col_scale(m.matrix, j, r.certificate), tol);
            }
            long double scale = 0;
            for (std::size_t i = 0; i < m.rhs.size(); ++i)
                scale += std::abs(static_cast<long double>(m.rhs[i]) * r.certificate[i]);
            double margin = dot(m.rhs, r.certificate);
            v.accepted = ok && margin > allowed(static_cast<double>(scale), tol);
            v.message = v.accepted ? "scaled Farkas certificate verified"
                                   : "scaled infeasibility verification failed";
            return v;
        }
        if (r.status == lp::reference::SolveStatus::unbounded) {
            if (r.primal.size() != m.matrix.columns || r.ray.size() != m.matrix.columns ||
                !finite(r.primal) || !finite(r.ray)) {
                v.message = "invalid unbounded payload";
                return v;
            }
            auto ax = m.multiply(r.primal), ad = m.multiply(r.ray);
            const auto primal_scale = row_scales(m.matrix, r.primal, m.rhs);
            const auto ray_scale = row_scales(m.matrix, r.ray, {});
            bool aok = true, rok = true;
            for (std::size_t i = 0; i < ax.size(); ++i) {
                double e = std::abs(ax[i] - m.rhs[i]);
                v.maximum_primal_violation = std::max(v.maximum_primal_violation, e);
                aok = aok && e <= allowed(primal_scale[i], tol);
                rok = rok && std::abs(ad[i]) <= allowed(ray_scale[i], tol);
            }
            for (double x : r.primal) {
                double e = std::max(0.0, -x);
                v.maximum_primal_violation = std::max(v.maximum_primal_violation, e);
                aok = aok && e <= allowed(std::abs(x), tol);
            }
            for (double d : r.ray)
                if (d < 0) {
                    v.message = "unbounded ray has negative component";
                    return v;
                }
            long double cs = 0;
            for (std::size_t j = 0; j < m.objective.size(); ++j)
                cs += std::abs(static_cast<long double>(m.objective[j]) * r.ray[j]);
            rok = rok && -dot(m.objective, r.ray) > allowed(static_cast<double>(cs), tol);
            v.accepted = aok && rok;
            v.message = v.accepted ? "scaled unbounded ray verified"
                                   : "scaled unboundedness verification failed";
            return v;
        }
        v.message = "non-conclusive status";
        return v;
    } catch (const std::exception& e) {
        v.message = std::string("verification failed: ") + e.what();
        return v;
    }
}
} // namespace markov_cero::verify
