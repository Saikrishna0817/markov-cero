#pragma once
#include "markov_cero/qp/model.hpp"
#include <algorithm>
#include <cmath>
namespace markov_cero::qp::detail {
inline bool local_kkt_gate(const QuadraticModel& model, const std::vector<double>& x,
    const std::vector<double>& y, const std::vector<double>& ax,
    const std::vector<double>& px, const std::vector<double>& aty,
    double absolute, double relative) {
    std::vector<long double> row_scale(model.num_constraints(), 0);
    for (std::size_t j = 0; j < model.num_variables(); ++j)
        for (std::size_t k = model.A.column_offsets[j]; k < model.A.column_offsets[j + 1]; ++k)
            row_scale[model.A.row_indices[k]] += std::abs(static_cast<long double>(model.A.values[k]) * x[j]);
    for (std::size_t i = 0; i < ax.size(); ++i) {
        if (!std::isfinite(ax[i]) || !std::isfinite(y[i])) return false;
        for (bool lower : {true, false}) {
            const double side = lower ? model.l[i] : model.u[i];
            if (!std::isfinite(side)) continue;
            const double violation = lower ? side - ax[i] : ax[i] - side;
            if (violation > absolute + relative * (row_scale[i] + std::abs(side))) return false;
        }
        if (y[i] != 0.0) {
            const double side = y[i] > 0 ? model.u[i] : model.l[i];
            if (!std::isfinite(side)) return false;
            const long double comp = std::abs(static_cast<long double>(y[i]) * (ax[i] - side));
            if (comp > absolute + relative * std::abs(y[i]) * (row_scale[i] + std::abs(side))) return false;
        }
    }
    for (std::size_t j = 0; j < x.size(); ++j) {
        const double residual = std::abs(px[j] + model.q[j] + aty[j]);
        const double allowance = absolute + relative * (std::abs(px[j]) + std::abs(model.q[j]) + std::abs(aty[j]));
        if (!std::isfinite(residual) || residual > allowance) return false;
    }
    return true;
}
} // namespace markov_cero::qp::detail
