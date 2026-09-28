#include "pdlp_internal.hpp"
namespace markov_cero::lp::first_order {
using namespace detail_pdlp;
PdlpResult iterate_pdlp(const model::Model& model, const model::Model& mdl,
    const scale::RuizScalers& scalers, const PdlpOptions& options,
    std::chrono::steady_clock::time_point t_start) {
    const auto m=model.matrix.row_count, n=model.matrix.column_count;
    const double obj_sign = (mdl.objective_sense == model::ObjectiveSense::maximize) ? -1.0 : 1.0;

    std::vector<double> c(n);
    double c_norm_inf = 1.0;
    for (std::size_t j = 0; j < n; ++j) {
        c[j] = obj_sign * mdl.objective[j];
        c_norm_inf = std::max(c_norm_inf, std::abs(c[j]));
    }

    std::vector<double> b_lo(m), b_hi(m);
    double b_norm_inf = 1.0;
    for (std::size_t i = 0; i < m; ++i) {
        b_lo[i] = (mdl.row_lower[i].kind == model::BoundKind::negative_infinity)
                      ? -1e300
                      : mdl.row_lower[i].value;
        b_hi[i] = (mdl.row_upper[i].kind == model::BoundKind::positive_infinity)
                      ? +1e300
                      : mdl.row_upper[i].value;
        if (std::abs(b_lo[i]) < 1e299) b_norm_inf = std::max(b_norm_inf, std::abs(b_lo[i]));
        if (std::abs(b_hi[i]) < 1e299) b_norm_inf = std::max(b_norm_inf, std::abs(b_hi[i]));
    }

    std::vector<double> row_norms(m, 0.0);
    std::vector<double> col_norms(n, 0.0);
    for (std::size_t col = 0; col < n; ++col) {
        for (std::size_t ptr = mdl.matrix.column_start[col];
             ptr < mdl.matrix.column_start[col + 1]; ++ptr) {
            const double val = std::abs(mdl.matrix.value[ptr]);
            col_norms[col] += val;
            row_norms[mdl.matrix.row_index[ptr]] += val;
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        if (col_norms[j] < 1e-12) col_norms[j] = 1.0;
    }
    for (std::size_t i = 0; i < m; ++i) {
        if (row_norms[i] < 1e-12) row_norms[i] = 1.0;
    }

    double omega = (options.initial_primal_weight > 0.0)
                       ? options.initial_primal_weight
                       : std::clamp(std::sqrt(c_norm_inf / b_norm_inf), 0.01, 100.0);
    double eta = std::clamp(options.step_size_reduction, 0.1, 0.99);

    std::vector<double> tau(n);
    std::vector<double> sigma(m);
    auto update_step_sizes = [&]() {
        for (std::size_t j = 0; j < n; ++j) {
            tau[j] = (eta / omega) / col_norms[j];
        }
        for (std::size_t i = 0; i < m; ++i) {
            sigma[i] = (eta * omega) / row_norms[i];
        }
    };
    update_step_sizes();

    std::vector<double> x(n, 0.0);
    std::vector<double> y(m, 0.0);
    std::vector<double> x_bar(n, 0.0);
    std::vector<double> x_avg(n, 0.0);
    std::vector<double> y_avg(m, 0.0);
    std::vector<double> delta_x(n, 0.0);

    for (std::size_t j = 0; j < n; ++j) {
        x[j] = project_bound(0.0, mdl.variable_lower[j], mdl.variable_upper[j]);
        x_bar[j] = x[j];
        x_avg[j] = x[j];
    }

    std::size_t iter = 0;
    double primal_infeas = std::numeric_limits<double>::max();
    double dual_infeas = std::numeric_limits<double>::max();
    double gap_val = std::numeric_limits<double>::max();
    std::size_t avg_count = 0;
    std::size_t iters_since_restart = 0;
    double last_restart_score = 1e300;

    struct ResidualCheckpoint {
        std::size_t iter{0};
        double score{0.0};
    };
    std::deque<ResidualCheckpoint> residual_history;
    std::size_t last_crossover_attempt_iter = 0;

    while (iter < options.max_iterations) {
        if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
            break;
        }
        auto At_y = spmv_t(mdl.matrix, y);
        std::vector<double> x_prev = x;
        double dx_norm_sq = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            double g = c[j] + At_y[j];
            double xnew = x[j] - tau[j] * g;
            double proj_x = project_bound(xnew, mdl.variable_lower[j], mdl.variable_upper[j]);
            const double dx = proj_x - x[j];
            delta_x[j] = dx;
            dx_norm_sq += col_norms[j] * dx * dx;
            x[j] = proj_x;
        }

        for (std::size_t j = 0; j < n; ++j) {
            x_bar[j] = 2.0 * x[j] - x_prev[j];
        }

        if (options.adaptive_step_size && iter % 10 == 0 && dx_norm_sq > 1e-14) {
            auto Adx = spmv(mdl.matrix, delta_x);
            double Adx_norm_sq = 0.0;
            for (std::size_t i = 0; i < m; ++i) {
                Adx_norm_sq += (Adx[i] * Adx[i]) / row_norms[i];
            }
            if (Adx_norm_sq > 1e-14) {
                double L_local = std::sqrt(Adx_norm_sq / dx_norm_sq);
                if (L_local > 1e-6) {
                    double target_eta = 0.95 / L_local;
                    if (target_eta < eta) {
                        eta = std::max(0.1, std::max(target_eta, eta * 0.8));
                        update_step_sizes();
                    } else if (target_eta > 1.05 * eta && eta < 0.99) {
                        eta = std::min(0.99, eta * 1.05);
                        update_step_sizes();
                    }
                }
            }
        }

        auto Ax_bar = spmv(mdl.matrix, x_bar);
        for (std::size_t i = 0; i < m; ++i) {
            double v = y[i] + sigma[i] * Ax_bar[i];
            double clamped = std::clamp(v / sigma[i], b_lo[i], b_hi[i]);
            y[i] = v - sigma[i] * clamped;
        }

        ++avg_count;
        for (std::size_t j = 0; j < n; ++j) {
            x_avg[j] += (x[j] - x_avg[j]) / static_cast<double>(avg_count);
        }
        for (std::size_t i = 0; i < m; ++i) {
            y_avg[i] += (y[i] - y_avg[i]) / static_cast<double>(avg_count);
        }

        ++iter;
        ++iters_since_restart;

        if (iter % options.restart_every == 0) {
            auto Ax_avg = spmv(mdl.matrix, x_avg);
            auto At_y_avg = spmv_t(mdl.matrix, y_avg);
            auto cur_res = compute_unscaled_residuals(
                model, x_avg, y_avg, Ax_avg, At_y_avg, scalers, options.ruiz_scaling);
            primal_infeas = cur_res.primal_infeas;
            dual_infeas = cur_res.dual_infeas;
            gap_val = cur_res.duality_gap;

            if (primal_infeas <= options.primal_tolerance &&
                dual_infeas <= options.dual_tolerance &&
                gap_val <= options.gap_tolerance) {
                const auto t_end = std::chrono::steady_clock::now();
                const double elapsed =
                    std::chrono::duration<double, std::milli>(t_end - t_start).count();
                PdlpResult res;
                res.status = PdlpStatus::optimal;
                res.primal = std::move(cur_res.x);
                res.dual = std::move(cur_res.y);
                res.objective = cur_res.objective;
                res.primal_infeasibility = primal_infeas;
                res.dual_infeasibility = dual_infeas;
                res.duality_gap = gap_val;
                res.dual_objective = cur_res.dual_objective;
                res.tolerance = options.primal_tolerance;
                res.iterations = iter;
                res.message = "PDLP converged";
                res.h2d_ms = 0.0;
                res.kernel_ms = elapsed;
                res.d2h_ms = 0.0;
                res.total_ms = elapsed;
                return res;
            }

            // Windowed stagnation detection & crossover (W = options.stagnation_window, theta = options.stagnation_threshold)
            if (options.enable_crossover) {
                residual_history.push_back({iter, cur_res.score});
                while (residual_history.size() > 1 &&
                       residual_history[1].iter <= iter - options.stagnation_window) {
                    residual_history.pop_front();
                }
                if (residual_history.front().iter + options.stagnation_window <= iter &&
                    iter >= last_crossover_attempt_iter + options.stagnation_window) {
                    const double old_score = residual_history.front().score;
                    const double rel_improve = (old_score - cur_res.score) / std::max(old_score, 1e-12);
                    if (rel_improve < (1.0 - options.stagnation_threshold)) {
                        last_crossover_attempt_iter = iter;
                        const auto attempt = try_dual_simplex_crossover(
                            model, mdl, x_avg, y_avg, scalers, options.ruiz_scaling, options, iter, t_start);
                        if (attempt.result.has_value()) {
                            return *attempt.result;
                        }
                        if (attempt.basis_singular) {
                            // D-15 (LOCKED): a singular extracted basis must
                            // neither crash the solve nor restart from a cold
                            // basis. Return the stagnated PDLP iterate with an
                            // explicit convergence note for the JSON output.
                            const auto t_end = std::chrono::steady_clock::now();
                            const double elapsed =
                                std::chrono::duration<double, std::milli>(t_end - t_start).count();
                            PdlpResult res;
                            res.status = PdlpStatus::iteration_limit;
                            res.primal = cur_res.x;
                            res.dual = cur_res.y;
                            res.objective = cur_res.objective;
                            res.primal_infeasibility = primal_infeas;
                            res.dual_infeasibility = dual_infeas;
                            res.duality_gap = gap_val;
                            res.dual_objective = cur_res.dual_objective;
                            res.tolerance = options.primal_tolerance;
                            res.iterations = iter;
                            res.message =
                                "PDLP stagnated; dual-simplex crossover basis singular";
                            res.convergence_note = "stagnated at tolerance floor";
                            res.h2d_ms = 0.0;
                            res.kernel_ms = elapsed;
                            res.d2h_ms = 0.0;
                            res.total_ms = elapsed;
                            return res;
                        }
                    }
                }
            }

            const double current_score = cur_res.score;
            bool do_restart = false;
            if (options.restart_strategy == RestartStrategy::fixed) {
                do_restart = true;
            } else if (options.restart_strategy == RestartStrategy::adaptive) {
                if (current_score <= options.restart_reduction_factor * last_restart_score ||
                    (iters_since_restart >= 5 * options.restart_every &&
                     current_score < last_restart_score)) {
                    do_restart = true;
                }
            }

            if (do_restart) {
                if (options.adaptive_primal_weight && primal_infeas > 1e-12 &&
                    dual_infeas > 1e-12) {
                    double ratio = std::sqrt(primal_infeas / dual_infeas);
                    ratio = std::clamp(ratio, 0.05, 20.0);
                    omega = std::clamp(
                        omega * std::pow(ratio, options.primal_weight_smoothing), 1e-6, 1e6);
                    update_step_sizes();
                }
                x = x_avg;
                y = y_avg;
                for (std::size_t j = 0; j < n; ++j) {
                    x_bar[j] = x[j];
                }
                avg_count = 0;
                iters_since_restart = 0;
                last_restart_score = current_score;
            }
        }
    }

    auto Ax_avg = spmv(mdl.matrix, x_avg);
    auto At_y_avg = spmv_t(mdl.matrix, y_avg);
    auto final_res = compute_unscaled_residuals(
        model, x_avg, y_avg, Ax_avg, At_y_avg, scalers, options.ruiz_scaling);


    const auto t_end = std::chrono::steady_clock::now();
    const double elapsed =
        std::chrono::duration<double, std::milli>(t_end - t_start).count();
    PdlpResult res;
    res.status = options.deadline && std::chrono::steady_clock::now() >= *options.deadline
                     ? PdlpStatus::resource_limit
                     : PdlpStatus::iteration_limit;
    res.primal = std::move(final_res.x);
    res.dual = std::move(final_res.y);
    res.objective = final_res.objective;
    res.primal_infeasibility = final_res.primal_infeas;
    res.dual_infeasibility = final_res.dual_infeas;
    res.duality_gap = final_res.duality_gap;
    res.dual_objective = final_res.dual_objective;
    res.tolerance = options.primal_tolerance;
    res.iterations = iter;
    res.message = res.status == PdlpStatus::resource_limit
                      ? "wall-clock deadline reached"
                      : "iteration limit reached";
    res.h2d_ms = 0.0;
    res.kernel_ms = elapsed;
    res.d2h_ms = 0.0;
    res.total_ms = elapsed;
    return res;
}
}
