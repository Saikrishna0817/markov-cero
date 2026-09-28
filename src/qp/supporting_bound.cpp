#include "markov_cero/qp/verifier.hpp"
#include <cmath>
namespace markov_cero::qp {
// Convex supporting-hyperplane lower bound over the variable box. General
// row multipliers use the OSQP sign convention. Unknown infinite infima fail
// closed instead of using the primal objective as a lower bound.
double supporting_lower_bound(const model::Model& model, const qp::QuadraticModel& q,
                        const qp::QpSolution& sol) {
    const auto px = q.P.multiply(sol.x);
    const auto m = model.matrix.row_count, n = model.matrix.column_count;
    std::vector<long double> reduced(n);
    long double bound = model.objective_offset, magnitude = std::abs(bound);
    for (std::size_t j = 0; j < n; ++j) {
        reduced[j] = q.q[j] + static_cast<long double>(px[j]);
        const long double term = -0.5L * sol.x[j] * px[j];
        bound += term; magnitude += std::abs(term);
    }
    for (std::size_t i = 0; i < m; ++i) {
        const double y = sol.y[i];
        if (y == 0.0) continue;
        const auto& side = y > 0.0 ? model.row_upper[i] : model.row_lower[i];
        if (!side.is_finite()) return -std::numeric_limits<double>::infinity();
        const long double term = -static_cast<long double>(y) * side.value;
        bound += term; magnitude += std::abs(term);
    }
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t k = model.matrix.column_start[j]; k < model.matrix.column_start[j + 1]; ++k)
            reduced[j] += static_cast<long double>(model.matrix.value[k]) * sol.y[model.matrix.row_index[k]];
        const auto& endpoint = reduced[j] >= 0.0L ? model.variable_lower[j] : model.variable_upper[j];
        if (reduced[j] == 0.0L) continue;
        if (!endpoint.is_finite()) return -std::numeric_limits<double>::infinity();
        const long double term = reduced[j] * endpoint.value;
        bound += term; magnitude += std::abs(term);
    }
    // Guard floating summation; this is a numerical bound, not an exact proof.
    bound -= 512.0L * std::numeric_limits<double>::epsilon() * (1.0L + magnitude);
    return std::isfinite(bound) ? static_cast<double>(bound) : -std::numeric_limits<double>::infinity();
}
}
