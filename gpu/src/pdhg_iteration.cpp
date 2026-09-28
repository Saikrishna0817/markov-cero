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
namespace detail {

void pdhg_primal_step_cpu(std::size_t n,
                          const double* tau,
                          const double* c,
                          const double* At_y,
                          const double* var_lower,
                          const double* var_upper,
                          double* x,
                          double* x_bar,
                          double* x_avg,
                          std::size_t avg_count) {
    const double inv_avg = (avg_count > 0) ? (1.0 / static_cast<double>(avg_count)) : 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        const double g = c[j] + At_y[j];
        const double x_prev = x[j];
        double x_new = std::clamp(x_prev - tau[j] * g, var_lower[j], var_upper[j]);
        x[j] = x_new;
        x_bar[j] = 2.0 * x_new - x_prev;
        if (inv_avg > 0.0) x_avg[j] += (x_new - x_avg[j]) * inv_avg;
    }
}

void pdhg_dual_step_cpu(std::size_t m,
                        const double* sigma,
                        const double* Ax_bar,
                        const double* row_lower,
                        const double* row_upper,
                        double* y,
                        double* y_avg,
                        std::size_t avg_count) {
    const double inv_avg = (avg_count > 0) ? (1.0 / static_cast<double>(avg_count)) : 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        const double sig = sigma[i];
        const double v = y[i] + sig * Ax_bar[i];
        const double clamped = std::clamp(v / sig, row_lower[i], row_upper[i]);
        const double y_new = v - sig * clamped;
        y[i] = y_new;
        if (inv_avg > 0.0) y_avg[i] += (y_new - y_avg[i]) * inv_avg;
    }
}

} // namespace detail

void pdhg_step_cpu(PdhgState& state, std::size_t avg_count) {
    if (!state.A || !state.At) throw std::invalid_argument("null matrix in state");
    const std::size_t n = state.num_variables, m = state.num_constraints;
    if (n == 0 || m == 0) return;

    spmv_transpose_cpu(*state.At, state.y, state.At_y);

    std::vector<double> tau(n), c(n), Aty(n), vlo(n), vhi(n), x(n), xbar(n), xavg(n);
    state.tau.download(tau.data(), n);
    state.c.download(c.data(), n);
    state.At_y.download(Aty.data(), n);
    state.var_lower.download(vlo.data(), n);
    state.var_upper.download(vhi.data(), n);
    state.x.download(x.data(), n);
    state.x_bar.download(xbar.data(), n);
    state.x_avg.download(xavg.data(), n);

    detail::pdhg_primal_step_cpu(n, tau.data(), c.data(), Aty.data(), vlo.data(),
                                 vhi.data(), x.data(), xbar.data(), xavg.data(), avg_count);

    state.x.upload(x.data(), n);
    state.x_bar.upload(xbar.data(), n);
    state.x_avg.upload(xavg.data(), n);

    spmv_cpu(*state.A, state.x_bar, state.Ax_bar);

    std::vector<double> sig(m), Axbar(m), rlo(m), rhi(m), y(m), yavg(m);
    state.sigma.download(sig.data(), m);
    state.Ax_bar.download(Axbar.data(), m);
    state.row_lower.download(rlo.data(), m);
    state.row_upper.download(rhi.data(), m);
    state.y.download(y.data(), m);
    state.y_avg.download(yavg.data(), m);

    detail::pdhg_dual_step_cpu(m, sig.data(), Axbar.data(), rlo.data(), rhi.data(),
                               y.data(), yavg.data(), avg_count);

    state.y.upload(y.data(), m);
    state.y_avg.upload(yavg.data(), m);
}

void pdhg_step(PdhgState& state, std::size_t avg_count) {
    if (!state.A || !state.At) throw std::invalid_argument("pdhg_step: null matrix");
    const std::size_t n = state.num_variables, m = state.num_constraints;
    if (n == 0 || m == 0) return;

#ifdef MARKOV_CERO_HAS_CUDA
    spmv_transpose(*state.At, state.y, state.At_y);
    detail::launch_pdhg_primal_step(
        n, state.tau.data(), state.c.data(), state.At_y.data(),
        state.var_lower.data(), state.var_upper.data(),
        state.x.data(), state.x_bar.data(), state.x_avg.data(), avg_count);
    spmv(*state.A, state.x_bar, state.Ax_bar);
    detail::launch_pdhg_dual_step(
        m, state.sigma.data(), state.Ax_bar.data(),
        state.row_lower.data(), state.row_upper.data(),
        state.y.data(), state.y_avg.data(), avg_count);
#else
    pdhg_step_cpu(state, avg_count);
#endif
}

void pdhg_run_iterations(PdhgState& state, std::size_t num_iters, std::size_t start_avg) {
    for (std::size_t k = 0; k < num_iters; ++k) pdhg_step(state, start_avg + k);
}

void pdhg_run_iterations_cpu(PdhgState& state, std::size_t num_iters, std::size_t start_avg) {
    for (std::size_t k = 0; k < num_iters; ++k) pdhg_step_cpu(state, start_avg + k);
}

}
