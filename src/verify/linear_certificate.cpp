#include "markov_cero/verify/linear_certificate.hpp"
#include "markov_cero/verify/primal_verifier.hpp"
#include <algorithm>
#include <cmath>
#include <new>
#include <stdexcept>
namespace markov_cero::verify {
LinearCertificate verify_linear_solution(const model::Model& m, const std::vector<double>& x,
    const std::vector<double>& y, double objective, double tolerance, bool relax_integrality,
    const core::Deadline& deadline) {
    LinearCertificate result;
    try {
        if (m.has_quadratic_objective || y.size() != m.matrix.row_count ||
            !std::isfinite(tolerance) || tolerance <= 0 || tolerance > 1e-4)
            throw std::invalid_argument("invalid linear certificate dimensions or tolerance");
        const auto primal = verify_primal(m, {x, objective}, {tolerance, tolerance},
                                         {tolerance, tolerance}, tolerance, !relax_integrality,
                                         deadline);
        const long double sign = m.objective_sense == model::ObjectiveSense::maximize ? -1.0L : 1.0L;
        long double bound = sign * m.objective_offset;
        for (std::size_t i = 0; i < y.size(); ++i) {
            if ((i & 1023U) == 0U && deadline.expired()) {
                result.message = "verification stopped: deadline exceeded";
                return result;
            }
            if (!std::isfinite(y[i])) throw std::invalid_argument("non-finite row dual");
            if (y[i] == 0.0) continue;
            const auto& side = y[i] > 0 ? m.row_upper[i] : m.row_lower[i];
            if (!side.is_finite()) throw std::invalid_argument("row dual has an infinite support value");
            bound -= static_cast<long double>(y[i]) * side.value;
        }
        for (std::size_t j = 0; j < m.matrix.column_count; ++j) {
            if ((j & 1023U) == 0U && deadline.expired()) {
                result.message = "verification stopped: deadline exceeded";
                return result;
            }
            long double reduced = sign * m.objective[j];
            for (std::size_t k = m.matrix.column_start[j]; k < m.matrix.column_start[j + 1]; ++k)
                reduced += static_cast<long double>(m.matrix.value[k]) * y[m.matrix.row_index[k]];
            if (!std::isfinite(reduced)) throw std::overflow_error("reduced cost overflow");
            if (reduced == 0.0L) continue;
            const auto& side = reduced > 0 ? m.variable_lower[j] : m.variable_upper[j];
            if (!side.is_finite()) throw std::invalid_argument("dual has no finite variable-box infimum");
            bound += reduced * side.value;
        }
        if (!std::isfinite(bound)) throw std::overflow_error("dual objective overflow");
        result.bound = static_cast<double>(sign * bound);
        const long double gap = sign * primal.recomputed_objective - bound;
        const long double scale = std::max({1.0L, std::abs(bound), std::abs(static_cast<long double>(objective))});
        result.relative_gap = static_cast<double>(std::abs(gap) / scale);
        result.accepted = primal.passed && result.relative_gap <= tolerance;
        result.message = result.accepted ? "original-space primal and weak-dual gap verified" : "original-space LP primal or dual gap rejected";
    } catch (const std::bad_alloc&) { throw; }
      catch (const std::length_error&) { throw; }
      catch (const std::exception& e) { result.message = e.what(); }
    return result;
}
} // namespace markov_cero::verify
