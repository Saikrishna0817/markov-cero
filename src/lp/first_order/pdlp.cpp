// markov-cero: sovereign first-order LP engine
// Primal-Dual Hybrid Gradient (PDHG / PDLP)
// Grounding: Chambolle & Pock (2011); Applegate et al. (2021)
//            "Practical Large-Scale Linear Programming using Primal-Dual Hybrid Gradient"

#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/gpu/pdhg_step.hpp"
#include "markov_cero/gpu/device.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/scale/ruiz_scaling.hpp"
#include "markov_cero/transform/canonicalize.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>

namespace markov_cero::lp::first_order {

namespace {

// Sparse matrix-vector product: y = A * x  (CSC format, row-wise accumulation)
std::vector<double> spmv(const model::SparseMatrixCSC& A, const std::vector<double>& x) {
    std::vector<double> y(A.row_count, 0.0);
    for (std::size_t col = 0; col < A.column_count; ++col) {
        const double xc = x[col];
        if (xc == 0.0) {
            continue;
        }
        for (std::size_t ptr = A.column_start[col]; ptr < A.column_start[col + 1]; ++ptr) {
            y[A.row_index[ptr]] += A.value[ptr] * xc;
        }
    }
    return y;
}

// Sparse transpose matrix-vector product: z = A^T * y
std::vector<double> spmv_t(const model::SparseMatrixCSC& A, const std::vector<double>& y) {
    std::vector<double> z(A.column_count, 0.0);
    for (std::size_t col = 0; col < A.column_count; ++col) {
        double s = 0.0;
        for (std::size_t ptr = A.column_start[col]; ptr < A.column_start[col + 1]; ++ptr) {
            s += A.value[ptr] * y[A.row_index[ptr]];
        }
        z[col] = s;
    }
    return z;
}

// Project x_j onto [l_j, u_j] respecting Bound types
inline double project_bound(double x, const model::Bound& lo, const model::Bound& hi) {
    double lo_val = (lo.kind == model::BoundKind::negative_infinity) ? -1e300 : lo.value;
    double hi_val = (hi.kind == model::BoundKind::positive_infinity) ? +1e300 : hi.value;
    return std::clamp(x, lo_val, hi_val);
}

struct UnscaledResiduals {
    double primal_infeas{0.0};
    double dual_infeas{0.0};
    double duality_gap{0.0};
    double score{0.0};
    std::vector<double> x;
    std::vector<double> y;
    double objective{0.0};
    // C-3: absolute dual objective in the engine's dual convention so callers
    // can form the complementarity gap |pobj - dobj| directly.
    double dual_objective{0.0};
};

UnscaledResiduals compute_unscaled_residuals(
    const model::Model& model,
    const std::vector<double>& x_avg,
    const std::vector<double>& y_avg,
    const std::vector<double>& Ax_avg,
    const std::vector<double>& At_y_avg,
    const scale::RuizScalers& scalers,
    bool ruiz_scaling) {
    const std::size_t m = model.matrix.row_count;
    const std::size_t n = model.matrix.column_count;
    const double obj_sign = (model.objective_sense == model::ObjectiveSense::maximize)
                                ? -1.0 : 1.0;

    UnscaledResiduals res;
    res.x.resize(n);
    res.y.resize(m);
    for (std::size_t j = 0; j < n; ++j) {
        res.x[j] = ruiz_scaling ? (x_avg[j] * scalers.col_scale[j]) : x_avg[j];
    }
    for (std::size_t i = 0; i < m; ++i) {
        res.y[i] = ruiz_scaling ? (y_avg[i] * scalers.row_scale[i]) : y_avg[i];
    }

    double max_prim_viol = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        const double ax_i = ruiz_scaling ? (Ax_avg[i] / scalers.row_scale[i]) : Ax_avg[i];
        const double lo = (model.row_lower[i].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.row_lower[i].value;
        const double hi = (model.row_upper[i].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.row_upper[i].value;
        const double proj = std::clamp(ax_i, lo, hi);
        const double r = std::abs(ax_i - proj);
        const double scale = 1.0 + std::max(std::abs(ax_i),
                                            std::abs(proj) < 1e299 ? std::abs(proj) : 0.0);
        max_prim_viol = std::max(max_prim_viol, r / scale);
    }
    for (std::size_t j = 0; j < n; ++j) {
        const double lo = (model.variable_lower[j].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.variable_lower[j].value;
        const double hi = (model.variable_upper[j].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.variable_upper[j].value;
        const double proj = std::clamp(res.x[j], lo, hi);
        const double r = std::abs(res.x[j] - proj);
        const double scale = 1.0 + std::max(std::abs(res.x[j]),
                                            std::abs(proj) < 1e299 ? std::abs(proj) : 0.0);
        max_prim_viol = std::max(max_prim_viol, r / scale);
    }
    res.primal_infeas = max_prim_viol;

    double c_scale = 1.0;
    double dual_res_sq = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        const double at_y = ruiz_scaling ? (At_y_avg[j] / scalers.col_scale[j]) : At_y_avg[j];
        const double c_orig = obj_sign * model.objective[j];
        c_scale = std::max(c_scale, std::abs(c_orig));
        const double g = c_orig + at_y;
        const double lo = (model.variable_lower[j].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.variable_lower[j].value;
        const double hi = (model.variable_upper[j].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.variable_upper[j].value;
        const double x_proj = std::clamp(res.x[j] - g, lo, hi);
        const double diff = res.x[j] - x_proj;
        dual_res_sq += diff * diff;
    }
    res.dual_infeas = std::sqrt(dual_res_sq) / c_scale;

    double primal_obj = model.objective_offset;
    for (std::size_t j = 0; j < n; ++j) {
        primal_obj += model.objective[j] * res.x[j];
    }
    res.objective = primal_obj;

    double dual_obj = model.objective_offset;
    for (std::size_t i = 0; i < m; ++i) {
        const double lo = (model.row_lower[i].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.row_lower[i].value;
        const double hi = (model.row_upper[i].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.row_upper[i].value;
        if (res.y[i] > 0.0 && hi < 1e299) {
            dual_obj -= res.y[i] * hi;
        } else if (res.y[i] < 0.0 && lo > -1e299) {
            dual_obj -= res.y[i] * lo;
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        const double at_y = ruiz_scaling ? (At_y_avg[j] / scalers.col_scale[j]) : At_y_avg[j];
        const double c_orig = obj_sign * model.objective[j];
        const double g = c_orig + at_y;
        const double lo = (model.variable_lower[j].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.variable_lower[j].value;
        const double hi = (model.variable_upper[j].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.variable_upper[j].value;
        if (g > 0.0 && lo > -1e299) {
            dual_obj += g * lo;
        } else if (g < 0.0 && hi < 1e299) {
            dual_obj += g * hi;
        }
    }
    res.duality_gap = std::abs(primal_obj - dual_obj) /
                      (1.0 + std::abs(primal_obj) + std::abs(dual_obj));
    res.dual_objective = dual_obj;
    res.score = std::max({res.primal_infeas, res.dual_infeas, res.duality_gap});
    return res;
}

// D-15: outcome of one crossover attempt. result holds the dual-simplex
// certified optimum; basis_singular marks the case where a candidate basis was
// extracted but rejected as singular (either by our pre-factorization or by the
// dual simplex warm-start factorization) -- the caller must then return the
// stagnated PDLP iterate with a convergence note instead of trying again.
struct CrossoverAttempt {
    std::optional<PdlpResult> result;
    bool basis_singular{false};
};

// D-15: build a candidate basis from the PDLP primal iterate x*. Classification
// follows complementary slackness (large z_j first, then the remaining
// columns), assembly is a rank-revealing greedy elimination, and the completed
// basis is factorized once so that a singular extraction is detected here
// rather than deep inside the dual simplex warm start.
enum class BasisExtraction { ok, missing, singular };

BasisExtraction extract_approximate_basis(const transform::CanonicalModel& canon,
                                           const std::vector<double>& z,
                                           lp::dual::BasisState& out_state) {
    const std::size_t m = canon.matrix.rows;
    const std::size_t n = canon.matrix.columns;

    // Rank-revealing column selection: prefer large z_j > 1e-7 first
    const double xi = 1e-7;
    std::vector<std::size_t> large;
    std::vector<std::size_t> rest;
    for (std::size_t j = 0; j < n; ++j) {
        (z[j] > xi ? large : rest).push_back(j);
    }
    std::sort(large.begin(), large.end(), [&](std::size_t a, std::size_t b) {
        return z[a] > z[b];
    });

    struct Pivot {
        std::size_t row;
        std::vector<double> column;
    };
    std::vector<Pivot> pivots;
    pivots.reserve(m);
    std::vector<char> row_used(m, 0);
    std::vector<std::size_t> basis;
    basis.reserve(m);

    const auto try_insert = [&](std::size_t j) {
        std::vector<double> c(m);
        double norm = 0.0;
        for (std::size_t i = 0; i < m; ++i) {
            c[i] = canon.matrix(i, j);
            norm = std::max(norm, std::abs(c[i]));
        }
        if (norm == 0.0) return;
        for (const Pivot& p : pivots) {
            const double factor = c[p.row];
            if (factor == 0.0) continue;
            for (std::size_t i = 0; i < m; ++i) {
                c[i] -= factor * p.column[i];
            }
        }
        const double threshold = 1e-10 * std::max(1.0, norm);
        std::size_t pivot_row = m;
        double best = threshold;
        for (std::size_t i = 0; i < m; ++i) {
            if (!row_used[i] && std::abs(c[i]) > best) {
                best = std::abs(c[i]);
                pivot_row = i;
            }
        }
        if (pivot_row == m) return;
        const double piv = c[pivot_row];
        for (std::size_t i = 0; i < m; ++i) {
            c[i] /= piv;
        }
        row_used[pivot_row] = 1;
        pivots.push_back(Pivot{pivot_row, std::move(c)});
        basis.push_back(j);
    };

    for (std::size_t j : large) {
        if (basis.size() == m) break;
        try_insert(j);
    }
    for (std::size_t j : rest) {
        if (basis.size() == m) break;
        try_insert(j);
    }
    if (basis.size() != m) return BasisExtraction::missing;

    // Verify basis non-singularity before the dual simplex warm start sees it.
    std::vector<std::vector<double>> basis_cols(m, std::vector<double>(m, 0.0));
    for (std::size_t p = 0; p < m; ++p) {
        const std::size_t col_idx = basis[p];
        for (std::size_t i = 0; i < m; ++i) {
            basis_cols[p][i] = canon.matrix(i, col_idx);
        }
    }
    auto basis_csc = linalg::SparseCsc::from_columns(m, basis_cols);
    try {
        (void)linalg::SparseLu::factorize(basis_csc, 1e-14);
    } catch (...) {
        return BasisExtraction::singular;
    }
    out_state = lp::dual::make_basis_state(canon, basis);
    return BasisExtraction::ok;
}

CrossoverAttempt try_dual_simplex_crossover(
    const model::Model& model,
    const model::Model& mdl,
    const std::vector<double>& x_scaled,
    const std::vector<double>& y_scaled,
    const scale::RuizScalers& scalers,
    bool ruiz_scaling,
    const PdlpOptions& options,
    std::size_t iter,
    std::chrono::steady_clock::time_point t_start) {
    (void)y_scaled;
    try {
        transform::CanonicalModel canon =
            transform::canonicalize(ruiz_scaling ? mdl : model);
        const std::size_t m = canon.matrix.rows;
        const std::size_t n = canon.matrix.columns;
        if (m == 0 || n < m) return {};

        // Map x_scaled to canonical z
        std::vector<double> z(n, 0.0);
        for (std::size_t j = 0; j < model.matrix.column_count; ++j) {
            const auto& vm = canon.record.variables[j];
            for (std::size_t q = 0; q < vm.canonical_index.size(); ++q) {
                const std::size_t c_idx = vm.canonical_index[q];
                const double mult = vm.multiplier[q];
                z[c_idx] = std::max(0.0, mult * (x_scaled[j] - vm.offset));
            }
        }
        for (std::size_t i = 0; i < m; ++i) {
            double ax = 0.0;
            for (std::size_t j = 0; j < canon.record.structural_variables; ++j) {
                ax += canon.matrix(i, j) * z[j];
            }
            // Check if row i has slack column
            for (std::size_t j = canon.record.structural_variables; j < n; ++j) {
                if (canon.matrix(i, j) == 1.0) {
                    z[j] = std::max(0.0, canon.rhs[i] - ax);
                    break;
                }
            }
        }

        lp::dual::BasisState warm_state;
        switch (extract_approximate_basis(canon, z, warm_state)) {
        case BasisExtraction::missing:
            return {};
        case BasisExtraction::singular:
            return {std::nullopt, true};
        case BasisExtraction::ok:
            break;
        }

        lp::dual::Options dual_options;
        dual_options.iteration_limit = options.crossover_simplex_limit;
        dual_options.feasibility_tolerance = 1e-7;
        dual_options.dual_tolerance = 1e-7;
        // D-15 (LOCKED): a singular extracted basis must never be re-attempted
        // from a cold basis -- extract_approximate_basis already rejected it
        // above and the caller returns the stagnated PDLP iterate with a
        // convergence note. For every other warm-start rejection (a candidate
        // basis that is nonsingular but not dual feasible) the certified cold
        // solve still applies: it does not need a perfect starting basis.
        dual_options.allow_cold_fallback = true;
        const auto dual_res = lp::dual::solve(canon, dual_options, warm_state);

        if (dual_res.solution.status == lp::reference::SolveStatus::numerical_failure &&
            dual_res.solution.message.find("singular") != std::string::npos) {
            // Warm-start factorization rejected the basis as singular (the
            // dense pre-check and this factorization can disagree near the
            // singularity tolerance).
            return {std::nullopt, true};
        }

        if (dual_res.solution.status == lp::reference::SolveStatus::optimal &&
            dual_res.solution.primal.size() == n) {
            PdlpResult res;
            res.status = PdlpStatus::optimal;
            res.primal = transform::reconstruct_primal(canon, dual_res.solution.primal);
            if (ruiz_scaling) {
                for (std::size_t j = 0; j < model.matrix.column_count; ++j) {
                    res.primal[j] *= scalers.col_scale[j];
                }
            }
            res.objective = model.objective_offset;
            for (std::size_t j = 0; j < model.matrix.column_count; ++j) {
                res.objective += model.objective[j] * res.primal[j];
            }

            // Reconstruct duals
            res.dual.assign(model.matrix.row_count, 0.0);
            std::size_t canon_r = 0;
            const double sign = canon.record.objective_sign;
            for (std::size_t i = 0; i < model.matrix.row_count; ++i) {
                const auto lo = (ruiz_scaling ? mdl : model).row_lower[i];
                const auto up = (ruiz_scaling ? mdl : model).row_upper[i];
                if (lo.is_finite() && up.is_finite() && lo.value == up.value) {
                    if (canon_r < dual_res.solution.dual.size())
                        res.dual[i] = sign * dual_res.solution.dual[canon_r++];
                } else {
                    if (up.is_finite() && canon_r < dual_res.solution.dual.size())
                        res.dual[i] += sign * dual_res.solution.dual[canon_r++];
                    if (lo.is_finite() && canon_r < dual_res.solution.dual.size())
                        res.dual[i] -= sign * dual_res.solution.dual[canon_r++];
                }
                if (ruiz_scaling && scalers.row_scale[i] != 0.0) {
                    res.dual[i] /= scalers.row_scale[i];
                }
            }

            // Compute residuals
            auto Ax = spmv(model.matrix, res.primal);
            auto At_y = spmv_t(model.matrix, res.dual);
            scale::RuizScalers dummy_scalers;
            auto unscaled = compute_unscaled_residuals(model, res.primal, res.dual, Ax, At_y, dummy_scalers, false);
            res.primal_infeasibility = unscaled.primal_infeas;
            res.dual_infeasibility = unscaled.dual_infeas;
            res.duality_gap = unscaled.duality_gap;
            res.dual_objective = unscaled.dual_objective;
            res.tolerance = 1e-7;
            res.iterations = iter;
            res.crossover_applied = true;
            res.condition_estimate = dual_res.solution.condition_estimate;
            res.message = "PDLP crossover: dual-simplex certified optimal";

            const auto t_end = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double, std::milli>(t_end - t_start).count();
            res.h2d_ms = 0.0;
            res.kernel_ms = elapsed;
            res.d2h_ms = 0.0;
            res.total_ms = elapsed;
            return {res, false};
        }
    } catch (...) {
        return {};
    }
    return {};
}

} // namespace

PdlpResult solve_pdlp(const model::Model& model, const PdlpOptions& options) {
    if (options.backend == Backend::gpu) {
        const bool has_device = gpu::is_gpu_available();
        auto result = gpu::solve_pdlp_gpu(model, options);
        result.backend_actually_used = has_device ? "cuda" : "cpu_fallback";
        return result;
    }

    const auto t_start = std::chrono::steady_clock::now();
    const std::size_t m = model.matrix.row_count;
    const std::size_t n = model.matrix.column_count;

    if (n == 0 || m == 0) {
        return PdlpResult{PdlpStatus::optimal, {}, {}, 0.0, 0.0, 0.0, 0.0,
                          options.primal_tolerance, 0, "trivial"};
    }

    model::Model mdl = model;
    scale::RuizScalers scalers;
    if (options.ruiz_scaling) {
        scale::RuizOptions scale_options;
        scale_options.max_iterations = options.ruiz_iterations;
        scale_options.deadline = options.deadline;
        scalers = scale::equilibrate_model(mdl, scale_options);
    }
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        PdlpResult res;
        res.status = PdlpStatus::resource_limit;
        res.message = "wall-clock deadline reached during PDLP preprocessing";
        return res;
    }

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

} // namespace markov_cero::lp::first_order
