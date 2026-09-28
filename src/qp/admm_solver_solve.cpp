#include "admm_solver_internal.hpp"
namespace markov_cero::qp {
using namespace detail_admm_solver;
QpSolution AdmmQpSolver::solve(const QuadraticModel& model) {
    if (!std::isfinite(options_.rho_init) || options_.rho_init <= 0 ||
        !std::isfinite(options_.sigma) || options_.sigma <= 0 ||
        !std::isfinite(options_.alpha) || options_.alpha <= 0 || options_.alpha >= 2 ||
        (options_.adaptive_rho && options_.adaptive_rho_interval == 0))
        throw std::invalid_argument("invalid ADMM penalty, relaxation or update interval");
    const auto start_time = std::chrono::steady_clock::now();
    QpSolution sol;
    auto deadline = options_.deadline;
    if (std::isfinite(options_.time_limit_seconds) &&
        options_.time_limit_seconds >= 0.0 && options_.time_limit_seconds < 1e8) {
        const auto local_deadline = start_time +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(options_.time_limit_seconds));
        if (!deadline || local_deadline < *deadline) deadline = local_deadline;
    }

    const std::size_t n = model.num_variables();
    const std::size_t m = model.num_constraints();
    if (n == 0) {
        sol.status = QpStatus::optimal;
        sol.objective_value = model.objective_offset;
        sol.y.assign(m, 0); sol.z.assign(m, 0);
        for (std::size_t i=0; i<m; ++i) {
            if (model.l[i] > 0 || model.u[i] < 0) {
                sol.status = QpStatus::primal_infeasible;
                sol.infeasibility_certificate.assign(m, 0);
                sol.infeasibility_certificate[i] = model.l[i] > 0 ? -1 : 1;
                break;
            }
        }
        return sol;
    }

    const auto convexity = assess_convexity(model.P, 1e-10, 5U * 1024U * 1024U,
                                            deadline);
    if (convexity.deadline_reached) {
        sol.status = QpStatus::time_limit;
        sol.message = convexity.message;
        return sol;
    }
    if (convexity.status == ConvexityStatus::non_convex) {
        sol.status = QpStatus::non_convex;
        sol.message = convexity.message;
        return sol;
    }
    if (convexity.status != ConvexityStatus::positive_semidefinite) {
        sol.status = QpStatus::unsupported;
        sol.message = convexity.message;
        return sol;
    }

    std::vector<double> x(n, 0.0);
    std::vector<double> z(m, 0.0);
    std::vector<double> y(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        z[i] = project_bound(0.0, model.l[i], model.u[i]);
    }

    std::vector<double> rho(m, options_.rho_init);

    KktSolver kkt;
    if (!kkt.factorize(model.P, model.A, options_.sigma, rho, deadline)) {
        sol.status = kkt.deadline_reached() ? QpStatus::time_limit :
                     kkt.fill_limit_reached() ? QpStatus::unsupported : QpStatus::numerical_error;
        if (kkt.deadline_reached()) sol.message = "deadline reached during QP KKT factorization";
        if (kkt.fill_limit_reached()) sol.message = "sparse QP KKT factor exceeds fill limit";
        return sol;
    }

    // W3/D-08 GPU residual path. LOCKED activation contract: explicit backend
    // request AND NNZ(P) > 100,000 AND a CUDA device. Anything else is a
    // silent CPU fallback (plan W4 Tier-1: never crash on a GPU request).
    bool gpu_residual_path = false;
    gpu::AdmmGpuContext gpu_ctx;
    if (gpu_requested_ &&
        model.P.values.size() > kGpuQpNnzThreshold && gpu::is_gpu_available()) {
        gpu_ctx = gpu::make_admm_gpu_context(model.P.column_offsets, model.P.row_indices,
                                             model.P.values, model.P.dimension, options_.rho_init);
        gpu_residual_path = gpu_ctx.valid();
        if (gpu_residual_path) {
            sol.gpu_path_active = true;


        }
    }

    std::vector<double> rhs_x(n, 0.0);
    std::vector<double> rhs_z(m, 0.0);
    std::vector<double> x_tilde(n, 0.0);
    std::vector<double> nu(m, 0.0);
    std::vector<double> z_tilde(m, 0.0);
    std::vector<double> x_hat(n, 0.0);
    std::vector<double> z_hat(m, 0.0);
    std::vector<double> x_prev(n, 0.0);
    std::vector<double> y_prev(m, 0.0);

    const double alpha = options_.alpha;
    const double sigma = options_.sigma;

    sol.status = QpStatus::iteration_limit;
    for (std::size_t iter = 0; iter < options_.max_iterations; ++iter) {
        sol.iterations = iter + 1;

        const auto now = std::chrono::steady_clock::now();
        const double elapsed =
            std::chrono::duration<double>(now - start_time).count();
        if (elapsed > options_.time_limit_seconds ||
            (deadline && now >= *deadline)) {
            sol.status = QpStatus::time_limit;
            break;
        }

        x_prev = x;
        y_prev = y;

        // 1. Construct RHS for KKT system
        for (std::size_t j = 0; j < n; ++j) {
            rhs_x[j] = sigma * x[j] - model.q[j];
        }
        for (std::size_t i = 0; i < m; ++i) {
            rhs_z[i] = z[i] - y[i] / rho[i];
        }

        // 2. Linear system solve
        kkt.solve(rhs_x, rhs_z, x_tilde, nu);

        // 3. Slack before relaxation
        for (std::size_t i = 0; i < m; ++i) {
            z_tilde[i] = z[i] + (nu[i] - y[i]) / rho[i];
        }

        // 4. Over-relaxation
        for (std::size_t j = 0; j < n; ++j) {
            x_hat[j] = alpha * x_tilde[j] + (1.0 - alpha) * x[j];
        }
        for (std::size_t i = 0; i < m; ++i) {
            z_hat[i] = alpha * z_tilde[i] + (1.0 - alpha) * z[i];
        }

        // 5. Projection onto constraint set [l, u]
        for (std::size_t i = 0; i < m; ++i) {
            const double arg = z_hat[i] + y[i] / rho[i];
            z[i] = project_bound(arg, model.l[i], model.u[i]);
            // Exact zero for an interior projection preserves the row normal cone.
            y[i] = rho[i] * (arg - z[i]);
        }

        // 6. Primal variable update
        x = x_hat;

        // 7. Residual computation and convergence checks. The dual residual's
        // Px product is the bandwidth-bound term; when the D-08 GPU path is
        // active it runs on the device, otherwise on the CPU SpMV.
        const std::vector<double> Ax = multiply_A(model.A, x);
        std::vector<double> r_prim(m, 0.0);
        for (std::size_t i = 0; i < m; ++i) {
            r_prim[i] = Ax[i] - z[i];
        }

        std::vector<double> Px;
        if (gpu_residual_path) {
            const gpu::DeviceBuffer<double> d_x(x);
            gpu::DeviceBuffer<double> d_px(n);
            gpu::admm_p_product(gpu_ctx, d_x, d_px);
            Px.resize(n);
            d_px.download(Px.data(), n);
        } else {
            Px = model.P.multiply(x);
        }
        const std::vector<double> ATy = multiply_AT(model.A, y);
        std::vector<double> r_dual(n, 0.0);
        for (std::size_t j = 0; j < n; ++j) {
            r_dual[j] = Px[j] + model.q[j] + ATy[j];
        }

        const double norm_prim = inf_norm(r_prim);
        const double norm_dual = inf_norm(r_dual);
        sol.primal_residual = norm_prim;
        sol.dual_residual = norm_dual;

        const double eps_prim = options_.absolute_tolerance +
                                options_.relative_tolerance *
                                    std::max(inf_norm(Ax), inf_norm(z));
        const double eps_dual = options_.absolute_tolerance +
                                options_.relative_tolerance *
                                    std::max({inf_norm(Px), inf_norm(ATy),
                                              inf_norm(model.q)});

        if (norm_prim <= eps_prim && norm_dual <= eps_dual &&
            detail::local_kkt_gate(model, x, y, Ax, Px, ATy,
                                   options_.absolute_tolerance, options_.relative_tolerance)) {
            sol.status = QpStatus::optimal;
            break;
        }

        if (iter % 10 == 0 && infeasibility_certificate(model, options_, x, y, x_prev, y_prev, sol)) break;

        // 9. Adaptive penalty parameter update (Boyd et al. 2011 §3.4.1)
        if (options_.adaptive_rho && iter > 0 &&
            (iter % options_.adaptive_rho_interval == 0)) {
            constexpr double mu = 10.0;
            constexpr double tau_incr = 2.0;
            constexpr double tau_decr = 2.0;
            constexpr double rho_min = 1e-6;
            constexpr double rho_max = 1e6;

            double factor = 1.0;
            if (norm_prim > mu * norm_dual) {
                factor = tau_incr;
            } else if (norm_dual > mu * norm_prim) {
                factor = 1.0 / tau_decr;
            }

            if (factor != 1.0) {
                bool changed = false;
                for (std::size_t i = 0; i < m; ++i) {
                    double new_rho = std::clamp(rho[i] * factor, rho_min, rho_max);
                    if (new_rho != rho[i]) {
                        rho[i] = new_rho;
                        changed = true;
                    }
                }
                if (changed) {
                    if (kkt.update_numeric(model.P, model.A, options_.sigma, rho, deadline)) {
                        ++sol.refactorization_count;
                    } else {
                        sol.status = kkt.deadline_reached() ? QpStatus::time_limit
                                                            : QpStatus::numerical_error;
                        if (kkt.deadline_reached())
                            sol.message = "deadline reached during QP KKT refactorization";
                        break;
                    }
                }
            }
        }
    }


    const auto end_time = std::chrono::steady_clock::now();
    sol.solve_time_seconds =
        std::chrono::duration<double>(end_time - start_time).count();

    // Compute objective value: (1/2) x^T P x + q^T x
    sol.x = std::move(x);
    sol.z = std::move(z);
    sol.y = std::move(y);

    const double energy = model.P.evaluate_energy(sol.x);
    double linear = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        linear += model.q[j] * sol.x[j];
    }
    const double internal_obj = 0.5 * energy + linear;

    if (model.sense == model::ObjectiveSense::maximize) {
        sol.objective_value = -internal_obj + model.objective_offset;
    } else {
        sol.objective_value = internal_obj + model.objective_offset;
    }

    // KKT LDL^T diagonal pivot-ratio proxy of the factorization that produced
    // this solution (reflects the final rho update, if any).
    sol.condition_estimate = kkt.condition_estimate();

    return sol;
}
}
