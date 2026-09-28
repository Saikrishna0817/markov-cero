#include "markov_cero/lp/interior/ipm.hpp"

// D-14 (LOCKED): the normal-equation Newton system A D A^T dy = rhs is
// factorized with the sparse Markowitz LU below; the former dense LU path is
// gone (it broke down around m = 200 rows).
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/scale/ruiz_scaling.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace markov_cero::lp::interior {
namespace {

using linalg::SparseCsc;
using linalg::SparseLu;
using linalg::SparseLuSymbolicAnalysis;

SparseCsc transpose_csc(const SparseCsc& A) {
    const std::size_t m = A.rows;
    const std::size_t n = A.columns;
    const std::size_t nnz = A.values.size();

    SparseCsc At;
    At.rows = n;
    At.columns = m;
    At.column_offsets.assign(m + 1, 0);
    At.row_indices.resize(nnz);
    At.values.resize(nnz);

    for (std::size_t p = 0; p < nnz; ++p) {
        ++At.column_offsets[A.row_indices[p] + 1];
    }
    for (std::size_t i = 0; i < m; ++i) {
        At.column_offsets[i + 1] += At.column_offsets[i];
    }

    std::vector<std::size_t> cur = At.column_offsets;
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t p = A.column_offsets[j]; p < A.column_offsets[j + 1]; ++p) {
            const std::size_t r = A.row_indices[p];
            const std::size_t dest = cur[r]++;
            At.row_indices[dest] = j;
            At.values[dest] = A.values[p];
        }
    }
    return At;
}

std::vector<double> spmv(const SparseCsc& A, const std::vector<double>& x) {
    std::vector<double> y(A.rows, 0.0);
    for (std::size_t j = 0; j < A.columns; ++j) {
        const double xj = x[j];
        if (xj == 0.0) continue;
        for (std::size_t p = A.column_offsets[j]; p < A.column_offsets[j + 1]; ++p) {
            y[A.row_indices[p]] += A.values[p] * xj;
        }
    }
    return y;
}

std::vector<double> spmv_t(const SparseCsc& A, const std::vector<double>& y) {
    std::vector<double> z(A.columns, 0.0);
    for (std::size_t j = 0; j < A.columns; ++j) {
        long double sum = 0.0L;
        for (std::size_t p = A.column_offsets[j]; p < A.column_offsets[j + 1]; ++p) {
            sum += static_cast<long double>(A.values[p]) * y[A.row_indices[p]];
        }
        z[j] = static_cast<double>(sum);
    }
    return z;
}

SparseCsc compute_sparse_normal_matrix(
    const SparseCsc& A,
    const SparseCsc& At,
    const std::vector<double>& d,
    double delta = 1e-12) {
    const std::size_t m = A.rows;
    SparseCsc M;
    M.rows = m;
    M.columns = m;
    M.column_offsets.reserve(m + 1);
    M.column_offsets.push_back(0);

    std::vector<double> accum(m, 0.0);
    std::vector<int> marker(m, -1);
    std::vector<std::size_t> active_rows;

    for (std::size_t k = 0; k < m; ++k) {
        active_rows.clear();
        const std::size_t at_start = At.column_offsets[k];
        const std::size_t at_end = At.column_offsets[k + 1];
        for (std::size_t p = at_start; p < at_end; ++p) {
            const std::size_t j = At.row_indices[p];
            const double s_val = At.values[p] * d[j];
            const std::size_t a_start = A.column_offsets[j];
            const std::size_t a_end = A.column_offsets[j + 1];
            for (std::size_t q = a_start; q < a_end; ++q) {
                const std::size_t i = A.row_indices[q];
                if (marker[i] != static_cast<int>(k)) {
                    marker[i] = static_cast<int>(k);
                    active_rows.push_back(i);
                    accum[i] = 0.0;
                }
                accum[i] += s_val * A.values[q];
            }
        }
        if (marker[k] != static_cast<int>(k)) {
            marker[k] = static_cast<int>(k);
            active_rows.push_back(k);
            accum[k] = delta;
        } else {
            accum[k] += delta;
        }

        std::sort(active_rows.begin(), active_rows.end());
        for (std::size_t r : active_rows) {
            // Products summed into accum[r] can cancel to exactly zero (e.g.
            // symmetric +/- coefficient pairs); an explicit zero would make
            // the CSC invalid for SparseLu's canonical-form validate. Skip it:
            // the entry carries no normal-equation information, and the
            // diagonal keeps its +delta regularizer.
            if (accum[r] == 0.0) {
                continue;
            }
            M.row_indices.push_back(r);
            M.values.push_back(accum[r]);
        }
        M.column_offsets.push_back(M.values.size());
    }
    return M;
}

struct Direction {
    std::vector<double> dx, dy, ds;
};

Direction newton_direction_sparse(
    const SparseCsc& a,
    const SparseCsc& M,
    const std::vector<double>& x,
    const std::vector<double>& s,
    const std::vector<double>& d,
    const std::vector<double>& r_p,
    const std::vector<double>& r_d,
    const std::vector<double>& tau,
    const SparseLu& factor) {
    const std::size_t m = a.rows;
    const std::size_t n = a.columns;

    std::vector<double> u(n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
        u[j] = tau[j] / s[j] - x[j] + d[j] * r_d[j];
    }
    const auto a_u = spmv(a, u);
    std::vector<double> rhs(m);
    for (std::size_t i = 0; i < m; ++i)
        rhs[i] = -r_p[i] - a_u[i];

    // Iterative refinement on normal equations: early exit at 1e-14
    const auto dy = factor.solve_refined(M, rhs, 2, 1e-14);
    const auto a_t_dy = spmv_t(a, dy);

    Direction out;
    out.dy = dy;
    out.dx.resize(n);
    out.ds.resize(n);
    for (std::size_t j = 0; j < n; ++j) {
        out.ds[j] = -a_t_dy[j] - r_d[j];
        out.dx[j] = tau[j] / s[j] - x[j] - d[j] * out.ds[j];
    }
    return out;
}

double max_step(const std::vector<double>& v, const std::vector<double>& dv,
                double fraction_to_boundary) {
    double alpha = 1.0;
    for (std::size_t j = 0; j < v.size(); ++j)
        if (dv[j] < 0.0)
            alpha = std::min(alpha, -v[j] / dv[j]);
    return fraction_to_boundary * alpha;
}

std::optional<std::vector<std::size_t>> crossover_basis_sparse(
    const SparseCsc& a, const std::vector<double>& x_ipm) {
    const std::size_t m = a.rows;
    const std::size_t n = a.columns;
    const double xi = 1e-7;

    if (m == 0 || n < m)
        return std::nullopt;

    std::vector<std::size_t> large;
    std::vector<std::size_t> rest;
    for (std::size_t j = 0; j < n; ++j) {
        const double v = j < x_ipm.size() ? x_ipm[j] : 0.0;
        (v > xi ? large : rest).push_back(j);
    }
    std::sort(large.begin(), large.end(), [&](std::size_t lhs, std::size_t rhs) {
        return x_ipm[lhs] > x_ipm[rhs];
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
        std::vector<double> c(m, 0.0);
        double norm = 0.0;
        for (std::size_t p = a.column_offsets[j]; p < a.column_offsets[j + 1]; ++p) {
            c[a.row_indices[p]] = a.values[p];
            norm = std::max(norm, std::abs(a.values[p]));
        }
        if (norm == 0.0)
            return;
        for (const Pivot& p : pivots) {
            const double factor = c[p.row];
            if (factor == 0.0)
                continue;
            for (std::size_t i = 0; i < m; ++i)
                c[i] -= factor * p.column[i];
        }
        const double threshold = 1e-10 * std::max(1.0, norm);
        std::size_t pivot_row = m;
        double best = threshold;
        for (std::size_t i = 0; i < m; ++i)
            if (!row_used[i] && std::abs(c[i]) > best) {
                best = std::abs(c[i]);
                pivot_row = i;
            }
        if (pivot_row == m)
            return;
        const double piv = c[pivot_row];
        for (std::size_t i = 0; i < m; ++i)
            c[i] /= piv;
        row_used[pivot_row] = 1;
        pivots.push_back(Pivot{pivot_row, std::move(c)});
        basis.push_back(j);
    };

    for (std::size_t j : large)
        if (basis.size() < m)
            try_insert(j);
    for (std::size_t j : rest)
        if (basis.size() < m)
            try_insert(j);

    if (basis.size() != m)
        return std::nullopt;

    // Verify basis with SparseLu
    std::vector<std::vector<double>> basis_cols(m, std::vector<double>(m, 0.0));
    for (std::size_t p = 0; p < m; ++p) {
        std::size_t col_idx = basis[p];
        for (std::size_t q = a.column_offsets[col_idx]; q < a.column_offsets[col_idx + 1]; ++q) {
            basis_cols[p][a.row_indices[q]] = a.values[q];
        }
    }
    auto basis_csc = SparseCsc::from_columns(m, basis_cols);
    try {
        (void)SparseLu::factorize(basis_csc, 1e-14);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    return basis;
}

} // namespace

Result solve(const transform::SparseCanonicalModel& model, const Options& options) {
    Result out;
    const SparseCsc& a_input = model.matrix;
    const std::size_t m = a_input.rows;
    const std::size_t n = a_input.columns;
    model.validate();

    if (m == 0 || n == 0) {
        out.status = lp::reference::SolveStatus::optimal;
        out.primal.assign(n, 0.0);
        out.dual.assign(m, 0.0);
        out.objective = model.objective_offset;
        out.message = "trivial model";
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

                transform::CanonicalModel scaled_copy = sparse_working.to_dense();
                const auto warm_state =
                    lp::dual::make_basis_state(scaled_copy, *candidate);
                const auto dual_res = lp::dual::solve(scaled_copy, dual_options, warm_state);
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
                        transform::CanonicalModel unscaled_dense = model.to_dense();
                        out.basis_state =
                            lp::dual::make_basis_state(unscaled_dense, dual_res.solution.basis);
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

Result solve(const transform::CanonicalModel& model, const Options& options) {
    transform::SparseCanonicalModel sparse;
    sparse.matrix.rows = model.matrix.rows;
    sparse.matrix.columns = model.matrix.columns;
    sparse.matrix.column_offsets.assign(model.matrix.columns + 1, 0);
    sparse.rhs = model.rhs;
    sparse.objective = model.objective;
    sparse.objective_offset = model.objective_offset;
    sparse.record = model.record;
    for (std::size_t j = 0; j < model.matrix.columns; ++j) {
        sparse.matrix.column_offsets[j + 1] = sparse.matrix.column_offsets[j];
        for (std::size_t i = 0; i < model.matrix.rows; ++i) {
            const double val = model.matrix.values[i * model.matrix.columns + j];
            if (val != 0.0) {
                sparse.matrix.row_indices.push_back(i);
                sparse.matrix.values.push_back(val);
                ++sparse.matrix.column_offsets[j + 1];
            }
        }
    }
    sparse.validate();
    auto res = solve(sparse, options);
    // If crossover succeeded and produced a basis, recreate basis_state against the dense model
    if (res.crossover_applied && res.basis_state.has_value()) {
        try {
            res.basis_state = lp::dual::make_basis_state(model, res.basis_state->basic_variables);
        } catch (...) {}
    }
    return res;
}

} // namespace markov_cero::lp::interior
