#include "ipm_internal.hpp"
namespace markov_cero::lp::interior {
using namespace detail_ipm;
Result solve(const transform::SparseCanonicalModel& model, const Options& options) {
    Result out;
    const SparseCsc& a_input = model.matrix;
    const std::size_t m = a_input.rows;
    const std::size_t n = a_input.columns;
    model.validate();

    if (n == 0) {
        const bool feasible = std::all_of(model.rhs.begin(), model.rhs.end(),
                                          [](double rhs) { return rhs == 0.0; });
        out.status = feasible ? lp::reference::SolveStatus::optimal
                              : lp::reference::SolveStatus::infeasible;
        out.dual.assign(m, 0.0);
        out.objective = model.objective_offset;
        out.message = feasible ? "empty feasible model" : "inconsistent empty model";
        return out;
    }
    if (m == 0) {
        const bool unbounded = std::any_of(model.objective.begin(), model.objective.end(),
                                            [](double cost) { return cost < 0.0; });
        out.status = unbounded ? lp::reference::SolveStatus::unbounded
                               : lp::reference::SolveStatus::optimal;
        out.primal.assign(n, 0.0);
        out.objective = model.objective_offset;
        out.message = unbounded ? "unbounded empty-row model" : "empty-row optimum";
        return out;
    }

    // Ruiz equilibration of A and b
    transform::SparseCanonicalModel sparse_working = model;
    sparse_working.record.objective_sign = 1.0;

    scale::RuizOptions ruiz_options;
    scale::RuizScalers scalers = scale::equilibrate(sparse_working, ruiz_options);
    const SparseCsc& a = sparse_working.matrix;
    const std::vector<double>& b = sparse_working.rhs;
    const std::vector<double>& c = sparse_working.objective;
    const SparseCsc at = transpose_csc(a);

    // Scale-aware starting point: x0 = max(1, ||b||_inf), s0 = max(1, ||c||_inf)
    const double max_b = [&] {
        double v = 1.0;
        for (double v_b : b)
            v = std::max(v, std::abs(v_b));
        return v;
    }();
    const double max_c = [&] {
        double v = 1.0;
        for (double v_c : c)
            v = std::max(v, std::abs(v_c));
        return v;
    }();

    std::vector<double> x(n, max_b), s(n, max_c), y(m, 0.0);
    auto r_p = spmv(a, x);
    for (std::size_t i = 0; i < m; ++i)
        r_p[i] -= b[i];
    auto r_d = spmv_t(a, y);
    for (std::size_t j = 0; j < n; ++j)
        r_d[j] += s[j] - c[j];

    const double b_norm = max_b;
    const double c_norm = max_c;

    // Precompute symbolic analysis for fill-reducing ordering across iterations
    const auto M_init = compute_sparse_normal_matrix(a, at, std::vector<double>(n, 1.0), 1e-12);
    const auto symbolic = SparseLu::analyze_sparsity(M_init, true);

    for (std::size_t iter = 0; iter < options.iteration_limit; ++iter) {
        if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
            out.status = lp::reference::SolveStatus::resource_limit;
            out.message = "ipm wall-clock deadline reached";
            break;
        }
        out.iterations = iter + 1;

        double mu = 0.0;
        for (std::size_t j = 0; j < n; ++j)
            mu += x[j] * s[j];
        mu /= static_cast<double>(n);

        double rp_inf = 0.0, rd_inf = 0.0;
        for (std::size_t i = 0; i < m; ++i)
            rp_inf = std::max(rp_inf, std::abs(r_p[i]));
        for (std::size_t j = 0; j < n; ++j)
            rd_inf = std::max(rd_inf, std::abs(r_d[j]));
        const double primal_tol = options.relative_tolerance * (1.0 + b_norm);
        const double dual_tol = options.relative_tolerance * (1.0 + c_norm);

        double cx = 0.0;
        for (std::size_t j = 0; j < n; ++j)
            cx += c[j] * x[j];
        const double gap_tol = options.relative_tolerance * (1.0 + std::abs(cx));

        if (rp_inf <= primal_tol && rd_inf <= dual_tol && mu <= gap_tol) {
            out.status = lp::reference::SolveStatus::optimal;
            break;
        }

        // Form sparse normal equations matrix M = A D A^T + delta * I
        std::vector<double> d(n, 0.0);
        for (std::size_t j = 0; j < n; ++j) {
            d[j] = std::clamp(x[j] / s[j], 1e-12, 1e12);
        }

        SparseCsc M = compute_sparse_normal_matrix(a, at, d, 1e-12);

        // Numeric factorization reusing cached symbolic analysis with regularizer fallback
        const auto factor = [&] {
            try {
                return SparseLu::factorize_numeric(M, symbolic, 1e-14);
            } catch (const std::exception&) {
                for (double delta : {1e-10, 1e-8, 1e-6, 1e-4, 1e-2}) {
                    M = compute_sparse_normal_matrix(a, at, d, delta);
                    try {
                        return SparseLu::factorize_numeric(M, symbolic, 1e-14);
                    } catch (const std::exception&) {}
                }
                throw std::runtime_error("ipm: normal matrix factorization failed");
            }
        }();
        // Condition telemetry: pivot-ratio proxy of the newest normal-equation
        // factorization (the system that actually drives the Newton step).
        out.condition_estimate = linalg::sparse_condition_estimate(factor.diagnostics());

        // Affine (predictor): tau = 0
        const std::vector<double> tau_zero(n, 0.0);
        const Direction affine = newton_direction_sparse(a, M, x, s, d, r_p, r_d, tau_zero, factor);
        const double alpha_p_aff = max_step(x, affine.dx, 1.0);
        const double alpha_d_aff = max_step(s, affine.ds, 1.0);

        double mu_affine = 0.0;
        for (std::size_t j = 0; j < n; ++j)
            mu_affine += (x[j] + alpha_p_aff * affine.dx[j]) *
                         (s[j] + alpha_d_aff * affine.ds[j]);
        mu_affine /= static_cast<double>(n);
        const double sigma =
            std::clamp(std::pow(std::max(mu_affine, 0.0) / mu, 3.0), 1e-8, 1.0 - 1e-8);

        // Corrector: tau = sigma * mu - dx_aff * ds_aff
        std::vector<double> tau(n);
        for (std::size_t j = 0; j < n; ++j)
            tau[j] = sigma * mu - affine.dx[j] * affine.ds[j];
        const Direction dir = newton_direction_sparse(a, M, x, s, d, r_p, r_d, tau, factor);

        // Decoupled primal / dual step sizes
        const double alpha_p = max_step(x, dir.dx, 0.995);
        const double alpha_d = max_step(s, dir.ds, 0.995);
        if (!(alpha_p > 1e-14 && alpha_d > 1e-14)) {
            break;
        }

        for (std::size_t j = 0; j < n; ++j) {
            x[j] += alpha_p * dir.dx[j];
            s[j] += alpha_d * dir.ds[j];
        }
        for (std::size_t i = 0; i < m; ++i)
            y[i] += alpha_d * dir.dy[i];

        r_p = spmv(a, x);
        for (std::size_t i = 0; i < m; ++i)
            r_p[i] -= b[i];
        r_d = spmv_t(a, y);
        for (std::size_t j = 0; j < n; ++j)
            r_d[j] += s[j] - c[j];

        for (std::size_t j = 0; j < n; ++j)
            if (!std::isfinite(x[j]) || !std::isfinite(s[j]))
                throw std::runtime_error("ipm: non-finite iterate");
    }

    // Map solution back to input canonical space
    lp::reference::Result scaled_result;
    scaled_result.status = out.status;
    const std::vector<double> x_scaled = x;
    scaled_result.primal = std::move(x);
    scaled_result.dual = std::move(y);
    scaled_result.objective = out.objective;
    scale::unscale_solution(scalers, scaled_result);
    out.primal = std::move(scaled_result.primal);
    out.dual = std::move(scaled_result.dual);

    out.objective = model.objective_offset;
    for (std::size_t j = 0; j < n; ++j)
        out.objective += model.objective[j] * out.primal[j];

    if (out.status == lp::reference::SolveStatus::optimal) {
        out.message = "ipm interior optimum";
    } else if (out.status == lp::reference::SolveStatus::resource_limit) {
        out.message = "ipm wall-clock deadline reached";
    } else {
        out.status = lp::reference::SolveStatus::iteration_limit;
        out.message = "ipm: iteration limit reached";
    }

    if (!options.enable_crossover)
        return out;
    if (out.status == lp::reference::SolveStatus::resource_limit)
        return out;

    // Crossover: candidate basis construction and dual simplex warm start
    {
        const auto candidate = crossover_basis_sparse(a, x_scaled);
        if (!candidate.has_value()) {
            if (out.status == lp::reference::SolveStatus::optimal) {
                out.message = "ipm optimum; crossover candidate singular or incomplete";
            }
            return out;
        }
        if (m <= 4096) {
            try {
                lp::dual::Options dual_options;
                dual_options.iteration_limit = std::max<std::size_t>(10000, options.iteration_limit * 50);
                dual_options.deadline = options.deadline;

                // Sparse crossover (sparse-lp-path.md §1): the warm state and
                // the dual solve run directly on the scaled CSC model; no
                // dense scaled copy and no dense unscaled copy are built.
                const auto warm_state =
                    lp::dual::make_basis_state(sparse_working, *candidate);
                const auto dual_res = lp::dual::solve(sparse_working, dual_options, warm_state);
                if (dual_res.solution.status == lp::reference::SolveStatus::optimal &&
                    dual_res.solution.primal.size() == n) {
                    lp::reference::Result vertex_scaled;
                    vertex_scaled.status = lp::reference::SolveStatus::optimal;
                    vertex_scaled.primal = dual_res.solution.primal;
                    vertex_scaled.dual = dual_res.solution.dual;
                    scale::unscale_solution(scalers, vertex_scaled);
                    out.status = lp::reference::SolveStatus::optimal;
                    out.primal = std::move(vertex_scaled.primal);
                    out.dual = std::move(vertex_scaled.dual);
                    double vertex_obj = model.objective_offset;
                    for (std::size_t j = 0; j < n; ++j)
                        vertex_obj += model.objective[j] * out.primal[j];
                    out.objective = vertex_obj;
                    out.crossover_applied = true;
                    out.message = "ipm optimum + crossover vertex (dual-certified)";
                    // The certified basis is the object carrying the final
                    // answer; prefer its condition proxy over the interior
                    // normal-matrix one.
                    if (dual_res.solution.condition_estimate > 0.0) {
                        out.condition_estimate = dual_res.solution.condition_estimate;
                    }
                    try {
                        out.basis_state =
                            lp::dual::make_basis_state(model, dual_res.solution.basis);
                    } catch (const std::exception&) {
                        out.basis_state.reset();
                    }
                } else if (out.status == lp::reference::SolveStatus::optimal) {
                    out.message = "ipm optimum; crossover candidate rejected by dual engine";
                }
            } catch (const std::exception&) {
                if (out.status == lp::reference::SolveStatus::optimal) {
                    out.message = "ipm optimum; crossover candidate not dual feasible"
                                  " (interior optimum kept)";
                }
            }
        }
    }
    return out;
}
}
