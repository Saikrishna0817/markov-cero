#include "markov_cero/gpu/pdhg_step.hpp"
#include "markov_cero/gpu/kernels.hpp"
#include "markov_cero/scale/ruiz_scaling.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <vector>

#ifdef MARKOV_CERO_HAS_CUDA
#include <cuda_runtime.h>
#endif

namespace markov_cero::gpu {
namespace {
inline double bound_val(const model::Bound& b, double def_inf) {
    return (b.kind == model::BoundKind::finite) ? b.value : def_inf;
}

}
PdhgResiduals evaluate_residuals(const PdhgState& state,
                                 const model::Model& original_model,
                                 const scale::RuizScalers* scalers,
                                 double* d2h_ms) {
    const std::size_t m = state.num_constraints, n = state.num_variables;
    if (m == 0 || n == 0) return PdhgResiduals{0.0, 0.0, 0.0, 0.0};

    std::vector<double> h_xavg(n), h_yavg(m);
    const auto t0 = std::chrono::steady_clock::now();
    state.x_avg.download(h_xavg.data(), n);
    state.y_avg.download(h_yavg.data(), m);
    if (d2h_ms) {
        *d2h_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
    }

    std::vector<double> x_orig(n), y_orig(m);
    for (std::size_t j = 0; j < n; ++j) {
        x_orig[j] = scalers ? (h_xavg[j] * scalers->col_scale[j]) : h_xavg[j];
    }
    for (std::size_t i = 0; i < m; ++i) {
        y_orig[i] = scalers ? (h_yavg[i] * scalers->row_scale[i]) : h_yavg[i];
    }

    std::vector<double> Ax_orig(m, 0.0);
    for (std::size_t col = 0; col < n; ++col) {
        const double xc = x_orig[col];
        if (xc != 0.0) {
            for (std::size_t ptr = original_model.matrix.column_start[col];
                 ptr < original_model.matrix.column_start[col + 1]; ++ptr) {
                Ax_orig[original_model.matrix.row_index[ptr]] +=
                    original_model.matrix.value[ptr] * xc;
            }
        }
    }

    double prim_viol = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        const double lo = bound_val(original_model.row_lower[i], -1e300);
        const double hi = bound_val(original_model.row_upper[i], +1e300);
        const double proj = std::clamp(Ax_orig[i], lo, hi);
        const double r = std::abs(Ax_orig[i] - proj);
        const double scale = 1.0 + std::max(std::abs(Ax_orig[i]),
                                            std::abs(proj) < 1e299 ? std::abs(proj) : 0.0);
        prim_viol = std::max(prim_viol, r / scale);
    }
    for (std::size_t j = 0; j < n; ++j) {
        const double lo = bound_val(original_model.variable_lower[j], -1e300);
        const double hi = bound_val(original_model.variable_upper[j], +1e300);
        const double proj = std::clamp(x_orig[j], lo, hi);
        const double r = std::abs(x_orig[j] - proj);
        const double scale = 1.0 + std::max(std::abs(x_orig[j]),
                                            std::abs(proj) < 1e299 ? std::abs(proj) : 0.0);
        prim_viol = std::max(prim_viol, r / scale);
    }

    std::vector<double> At_yorig(n, 0.0);
    for (std::size_t col = 0; col < n; ++col) {
        double s = 0.0;
        for (std::size_t ptr = original_model.matrix.column_start[col];
             ptr < original_model.matrix.column_start[col + 1]; ++ptr) {
            s += original_model.matrix.value[ptr] * y_orig[original_model.matrix.row_index[ptr]];
        }
        At_yorig[col] = s;
    }

    const double obj_sign = (original_model.objective_sense == model::ObjectiveSense::maximize)
                                ? -1.0 : 1.0;
    double dual_res_sq = 0.0, c_scale = 1.0;
    for (std::size_t j = 0; j < n; ++j) {
        const double c_j = obj_sign * original_model.objective[j];
        c_scale = std::max(c_scale, std::abs(c_j));
        const double g = c_j + At_yorig[j];
        const double lo = bound_val(original_model.variable_lower[j], -1e300);
        const double hi = bound_val(original_model.variable_upper[j], +1e300);
        const double x_proj = std::clamp(x_orig[j] - g, lo, hi);
        const double diff = x_orig[j] - x_proj;
        dual_res_sq += diff * diff;
    }
    const double dual_viol = std::sqrt(dual_res_sq) / c_scale;

    double prim_obj = original_model.objective_offset;
    for (std::size_t j = 0; j < n; ++j) prim_obj += original_model.objective[j] * x_orig[j];

    double dual_obj = obj_sign * original_model.objective_offset;
    for (std::size_t i = 0; i < m; ++i) {
        const double lo = bound_val(original_model.row_lower[i], -1e300);
        const double hi = bound_val(original_model.row_upper[i], +1e300);
        if (y_orig[i] > 0.0 && hi < 1e299) dual_obj -= y_orig[i] * hi;
        else if (y_orig[i] < 0.0 && lo > -1e299) dual_obj -= y_orig[i] * lo;
    }
    for (std::size_t j = 0; j < n; ++j) {
        const double c_j = obj_sign * original_model.objective[j];
        const double g = c_j + At_yorig[j];
        const double lo = bound_val(original_model.variable_lower[j], -1e300);
        const double hi = bound_val(original_model.variable_upper[j], +1e300);
        if (g > 0.0 && lo > -1e299) dual_obj += g * lo;
        else if (g < 0.0 && hi < 1e299) dual_obj += g * hi;
    }

    dual_obj *= obj_sign;
    const double gap_viol = std::abs(prim_obj - dual_obj) /
                            (1.0 + std::abs(prim_obj) + std::abs(dual_obj));
    const double score = std::max({prim_viol, dual_viol, gap_viol});
    return PdhgResiduals{prim_viol, dual_viol, gap_viol, score};
}

}
