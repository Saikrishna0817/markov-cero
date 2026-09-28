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

constexpr double kInfinitySentinel = 1e300;

double bound_to_double(const model::Bound& b, bool is_upper) {
    if (is_upper) {
        return (b.kind == model::BoundKind::positive_infinity) ? kInfinitySentinel : b.value;
    }
    return (b.kind == model::BoundKind::negative_infinity) ? -kInfinitySentinel : b.value;
}

} // namespace

PdhgState create_pdhg_state(const model::Model& model,
                            const DeviceCsr& A,
                            const DeviceCsr& At,
                            double step_size_reduction,
                            double primal_weight) {
    const std::size_t m = model.matrix.row_count;
    const std::size_t n = model.matrix.column_count;

    if (A.rows() != m || A.cols() != n || At.rows() != n || At.cols() != m) {
        throw std::invalid_argument("create_pdhg_state: matrix dimension mismatch");
    }

    const double obj_sign = (model.objective_sense == model::ObjectiveSense::maximize) ? -1.0 : 1.0;
    std::vector<double> h_c(n), h_var_lo(n), h_var_hi(n), h_x0(n);
    for (std::size_t j = 0; j < n; ++j) {
        h_c[j] = obj_sign * model.objective[j];
        h_var_lo[j] = bound_to_double(model.variable_lower[j], false);
        h_var_hi[j] = bound_to_double(model.variable_upper[j], true);
        h_x0[j] = std::clamp(0.0, h_var_lo[j], h_var_hi[j]);
    }

    std::vector<double> h_row_lo(m), h_row_hi(m);
    for (std::size_t i = 0; i < m; ++i) {
        h_row_lo[i] = bound_to_double(model.row_lower[i], false);
        h_row_hi[i] = bound_to_double(model.row_upper[i], true);
    }

    std::vector<double> col_norms(n, 0.0), row_norms(m, 0.0);
    for (std::size_t col = 0; col < n; ++col) {
        const std::size_t start = model.matrix.column_start[col];
        const std::size_t end = model.matrix.column_start[col + 1];
        for (std::size_t ptr = start; ptr < end; ++ptr) {
            const double val = std::abs(model.matrix.value[ptr]);
            col_norms[col] += val;
            row_norms[model.matrix.row_index[ptr]] += val;
        }
    }
    for (std::size_t j = 0; j < n; ++j) if (col_norms[j] < 1e-12) col_norms[j] = 1.0;
    for (std::size_t i = 0; i < m; ++i) if (row_norms[i] < 1e-12) row_norms[i] = 1.0;

    const double eta = std::clamp(step_size_reduction, 0.1, 0.99);
    const double omega = std::clamp(primal_weight, 1e-6, 1e6);

    std::vector<double> h_tau(n, 1.0), h_sigma(m, 1.0);
    for (std::size_t j = 0; j < n; ++j) h_tau[j] = (eta / omega) / col_norms[j];
    for (std::size_t i = 0; i < m; ++i) h_sigma[i] = (eta * omega) / row_norms[i];

    PdhgState state;
    state.A = &A;
    state.At = &At;
    state.num_variables = n;
    state.num_constraints = m;
    state.col_norms = std::move(col_norms);
    state.row_norms = std::move(row_norms);
    state.eta = eta;
    state.omega = omega;

    state.c = DeviceBuffer<double>(h_c);
    state.var_lower = DeviceBuffer<double>(h_var_lo);
    state.var_upper = DeviceBuffer<double>(h_var_hi);
    state.row_lower = DeviceBuffer<double>(h_row_lo);
    state.row_upper = DeviceBuffer<double>(h_row_hi);
    state.tau = DeviceBuffer<double>(h_tau);
    state.sigma = DeviceBuffer<double>(h_sigma);

    state.x = DeviceBuffer<double>(h_x0);
    state.x_bar = DeviceBuffer<double>(h_x0);
    state.x_avg = DeviceBuffer<double>(h_x0);
    state.y = DeviceBuffer<double>(std::vector<double>(m, 0.0));
    state.y_avg = DeviceBuffer<double>(std::vector<double>(m, 0.0));

    state.At_y = DeviceBuffer<double>(n);
    state.Ax_bar = DeviceBuffer<double>(m);
    return state;
}

void pdhg_update_step_sizes(PdhgState& state, double eta, double omega,
                            double* h2d_ms) {
    const std::size_t n = state.num_variables, m = state.num_constraints;
    if (n == 0 || m == 0) return;
    state.eta = std::clamp(eta, 0.1, 0.99);
    state.omega = std::clamp(omega, 1e-6, 1e6);

    std::vector<double> h_tau(n), h_sigma(m);
    for (std::size_t j = 0; j < n; ++j) h_tau[j] = (state.eta / state.omega) / state.col_norms[j];
    for (std::size_t i = 0; i < m; ++i) h_sigma[i] = (state.eta * state.omega) / state.row_norms[i];
    const auto t0 = std::chrono::steady_clock::now();
    state.tau.upload(h_tau.data(), n);
    state.sigma.upload(h_sigma.data(), m);
    if (h2d_ms) {
        *h2d_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
    }
}

void pdhg_restart(PdhgState& state) {
    const std::size_t n = state.num_variables, m = state.num_constraints;
    if (n == 0 || m == 0) return;
    state.x.copy_from(state.x_avg);
    state.x_bar.copy_from(state.x_avg);
    state.y.copy_from(state.y_avg);
}

}
