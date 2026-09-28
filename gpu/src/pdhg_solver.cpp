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
markov_cero::lp::first_order::PdlpResult solve_pdlp_gpu(
    const model::Model& model,
    const markov_cero::lp::first_order::PdlpOptions& options) {
    using namespace markov_cero::lp::first_order;
    const auto t_total_start = std::chrono::steady_clock::now();
    double h2d_ms = 0.0, kernel_ms = 0.0, d2h_ms = 0.0;
    const std::size_t m = model.matrix.row_count, n = model.matrix.column_count;
    if (m == 0 || n == 0) {
        auto cpu = options; cpu.backend = Backend::cpu;
        return solve_pdlp(model, cpu);
    }

    model::Model scaled_model = model;
    scale::RuizScalers scalers;
    if (options.ruiz_scaling) {
        scale::RuizOptions scale_options;
        scale_options.max_iterations = options.ruiz_iterations;
        scale_options.deadline = options.deadline;
        scalers = scale::equilibrate_model(scaled_model, scale_options);
    }
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        PdlpResult res;
        res.status = PdlpStatus::resource_limit;
        res.message = "wall-clock deadline reached during GPU PDLP preprocessing";
        return res;
    }

    double c_norm_inf = 1.0, b_norm_inf = 1.0;
    for (std::size_t j = 0; j < n; ++j) {
        c_norm_inf = std::max(c_norm_inf, std::abs(scaled_model.objective[j]));
    }
    for (std::size_t i = 0; i < m; ++i) {
        if (scaled_model.row_lower[i].kind == model::BoundKind::finite) {
            b_norm_inf = std::max(b_norm_inf, std::abs(scaled_model.row_lower[i].value));
        }
        if (scaled_model.row_upper[i].kind == model::BoundKind::finite) {
            b_norm_inf = std::max(b_norm_inf, std::abs(scaled_model.row_upper[i].value));
        }
    }
    double omega = (options.initial_primal_weight > 0.0)
                       ? options.initial_primal_weight
                       : std::clamp(std::sqrt(c_norm_inf / b_norm_inf), 0.01, 100.0);

    const auto t_h2d_0 = std::chrono::steady_clock::now();
    DeviceCsr A = DeviceCsr::from_csc(scaled_model.matrix);
    DeviceCsr At = DeviceCsr::transpose_from_csc(scaled_model.matrix);
    PdhgState state = create_pdhg_state(
        scaled_model, A, At, options.step_size_reduction, omega);
#ifdef MARKOV_CERO_HAS_CUDA
    cudaDeviceSynchronize();
#endif
    h2d_ms += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t_h2d_0).count();

    std::size_t iter = 0, avg_count = 0, iters_since_restart = 0;
    const scale::RuizScalers* p_scalers = options.ruiz_scaling ? &scalers : nullptr;
    auto initial_resids = evaluate_residuals(state, model, p_scalers, &d2h_ms);
    double last_score = initial_resids.score;
    PdhgResiduals last_resids = initial_resids;

    const std::size_t check_interval = std::max<std::size_t>(1, options.restart_every);

    auto make_result = [&](PdlpStatus st, const char* msg) {
        std::vector<double> h_x(n), h_y(m);
        const auto t_d_0 = std::chrono::steady_clock::now();
        state.x_avg.download(h_x.data(), n);
        state.y_avg.download(h_y.data(), m);
        d2h_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t_d_0).count();

        if (options.ruiz_scaling) {
            scale::unscale_model_solution(scalers, h_x, h_y);
        }
        double final_obj = model.objective_offset;
        for (std::size_t j = 0; j < n; ++j) final_obj += model.objective[j] * h_x[j];

        const double total_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t_total_start).count();
        return PdlpResult{st, std::move(h_x), std::move(h_y), final_obj,
                          last_resids.primal_infeasibility,
                          last_resids.dual_infeasibility,
                          last_resids.duality_gap,
                          options.primal_tolerance, iter, msg,
                          h2d_ms, kernel_ms, d2h_ms, total_ms};
    };

    while (iter < options.max_iterations) {
        const std::size_t chunk = std::min(check_interval, options.max_iterations - iter);
        const auto t_k_0 = std::chrono::steady_clock::now();
        for (std::size_t k = 0; k < chunk; ++k) {
            if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline)
                return make_result(PdlpStatus::resource_limit, "GPU PDLP deadline reached");
            pdhg_step(state, ++avg_count);
        }
#ifdef MARKOV_CERO_HAS_CUDA
        cudaDeviceSynchronize();
#endif
        kernel_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t_k_0).count();
        iter += chunk;
        iters_since_restart += chunk;

        last_resids = evaluate_residuals(state, model, p_scalers, &d2h_ms);

        if (last_resids.primal_infeasibility <= options.primal_tolerance &&
            last_resids.dual_infeasibility <= options.dual_tolerance &&
            last_resids.duality_gap <= options.gap_tolerance) {
            return make_result(PdlpStatus::optimal, "GPU PDLP converged");
        }

        bool do_restart = false;
        if (options.restart_strategy == RestartStrategy::fixed) {
            do_restart = true;
        } else if (options.restart_strategy == RestartStrategy::adaptive) {
            if (last_resids.score <= options.restart_reduction_factor * last_score ||
                (iters_since_restart >= 5 * check_interval && last_resids.score < last_score)) {
                do_restart = true;
            }
        }

        if (do_restart) {
            if (options.adaptive_primal_weight &&
                last_resids.primal_infeasibility > 1e-12 &&
                last_resids.dual_infeasibility > 1e-12) {
                double ratio = std::sqrt(last_resids.primal_infeasibility /
                                         last_resids.dual_infeasibility);
                ratio = std::clamp(ratio, 0.05, 20.0);
                state.omega = std::clamp(
                    state.omega * std::pow(ratio, options.primal_weight_smoothing),
                    1e-6, 1e6);
                pdhg_update_step_sizes(state, state.eta, state.omega, &h2d_ms);
            }
            const auto t_rst_0 = std::chrono::steady_clock::now();
            pdhg_restart(state);
#ifdef MARKOV_CERO_HAS_CUDA
            cudaDeviceSynchronize();
#endif
            kernel_ms += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - t_rst_0).count();
            avg_count = 0;
            iters_since_restart = 0;
            last_score = last_resids.score;
        }
    }

    return make_result(PdlpStatus::iteration_limit, "GPU PDLP iteration limit reached");
}

}
