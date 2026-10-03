#include "ipm_internal.hpp"
namespace markov_cero::lp::interior {
using namespace detail_ipm;
Result run_crossover(const transform::SparseCanonicalModel& model,
                     const transform::SparseCanonicalModel& sparse_working,
                     const std::vector<double>& x_scaled,
                     const scale::RuizScalers& scalers, const Options& options, Result out) {
    const SparseCsc& a = sparse_working.matrix;
    const std::vector<double>& b = sparse_working.rhs;
    const std::vector<double>& c = sparse_working.objective;
    const std::size_t m = a.rows;
    const std::size_t n = a.columns;
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
        bool crossover_primal_ok = false;
        // Exact vertex acceptance: when the crossover basis already IS the
        // optimal vertex, solve B x_B = b and B^T y = c_B from scratch and
        // accept only if primal and dual feasibility both hold at reference
        // tolerances. The dual warm start below remains a certifier, but it
        // must not be the only route to the vertex it is handed: scsd1/scsd6
        // deliver an interior optimum whose crossover basis solves exactly,
        // yet the warm-start simplex rejects the basis numerically and the
        // interior point is then (correctly) failed by the engine witness
        // gate. Nothing is bypassed — the engine gate re-verifies the final
        // canonical witness independently.
        {
            const double feas_tol = 1e-9;
            const double dual_tol = 1e-9;
            try {
                std::vector<std::vector<double>> basis_cols(m, std::vector<double>(m, 0.0));
                for (std::size_t p = 0; p < m; ++p) {
                    const std::size_t j = (*candidate)[p];
                    for (std::size_t q = a.column_offsets[j]; q < a.column_offsets[j + 1]; ++q)
                        basis_cols[p][a.row_indices[q]] = a.values[q];
                }
                const auto basis_csc = SparseCsc::from_columns(m, basis_cols);
                const auto lu = SparseLu::factorize(basis_csc, 1e-14);
                // Primal: x_B = B^{-1} b; every basic value must respect x >= 0.
                const auto x_B = lu.solve_refined(basis_csc, b, 3, 1e-14);
                bool primal_ok = true;
                for (double v : x_B) {
                    if (!std::isfinite(v) || v < -feas_tol) primal_ok = false;
                }
                crossover_primal_ok = primal_ok;
                // Dual: B^T y = c_B with one residual correction.
                std::vector<double> c_B(m);
                for (std::size_t p = 0; p < m; ++p) c_B[p] = c[(*candidate)[p]];
                auto y_vertex = lu.solve_transpose(c_B);
                {
                    std::vector<double> residual(m);
                    for (std::size_t p = 0; p < m; ++p) {
                        long double acc = 0;
                        for (std::size_t i = 0; i < m; ++i)
                            acc += static_cast<long double>(basis_cols[p][i]) * y_vertex[i];
                        residual[p] = static_cast<double>(static_cast<long double>(c_B[p]) - acc);
                    }
                    const auto correction = lu.solve_transpose(residual);
                    for (std::size_t i = 0; i < m; ++i) y_vertex[i] += correction[i];
                }
                // Dual feasibility: rc = c - A^T y >= 0 within reference scaling.
                const auto at_y = spmv_t(a, y_vertex);
                std::vector<double> x_vertex(n, 0.0);
                for (std::size_t p = 0; p < m; ++p) x_vertex[(*candidate)[p]] = x_B[p];
                bool dual_ok = primal_ok;
                for (std::size_t j = 0; j < n; ++j) {
                    const double rc = c[j] - at_y[j];
                    long double scale = std::abs(static_cast<long double>(c[j]));
                    for (std::size_t q = a.column_offsets[j]; q < a.column_offsets[j + 1]; ++q)
                        scale += std::abs(static_cast<long double>(a.values[q]) *
                                          y_vertex[a.row_indices[q]]);
                    const double z = static_cast<double>(scale);
                    const double allowed =
                        dual_tol * z + 64.0 * std::numeric_limits<double>::epsilon() *
                                           std::max(1.0, z);
                    if (rc < -allowed) dual_ok = false;
                }
                if (primal_ok && dual_ok) {
                    lp::reference::Result vertex_scaled;
                    vertex_scaled.status = lp::reference::SolveStatus::optimal;
                    vertex_scaled.primal = std::move(x_vertex);
                    vertex_scaled.dual = std::move(y_vertex);
                    scale::unscale_solution(scalers, vertex_scaled);
                    out.status = lp::reference::SolveStatus::optimal;
                    out.primal = std::move(vertex_scaled.primal);
                    out.dual = std::move(vertex_scaled.dual);
                    out.objective = model.objective_offset;
                    for (std::size_t j = 0; j < n; ++j)
                        out.objective += model.objective[j] * out.primal[j];
                    out.crossover_applied = true;
                    out.message = "ipm optimum + crossover vertex (basis solve)";
                    out.condition_estimate = linalg::sparse_condition_estimate(lu.diagnostics());
                    try {
                        out.basis_state = lp::dual::make_basis_state(model, *candidate);
                    } catch (const std::exception&) {
                        out.basis_state.reset();
                    }
                    return out;
                }
            } catch (const std::exception&) {
                // Basis unusable for an exact solve: fall through to the dual
                // warm start below.
            }
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
        // Reference primal warm polish: the crossover basis is primal
        // feasible (checked above) but not yet dual feasible, so the primal
        // simplex pivots it to optimality from that exact start. Runs only
        // when both certifiers above declined; reference::solve certify()s
        // its own result on this scaled model and the engine witness gate
        // re-verifies the canonical answer afterwards.
        if (!out.crossover_applied && crossover_primal_ok) {
            try {
                lp::reference::Options ref_options;
                ref_options.deadline = options.deadline;
                // Dantzig pricing for the polish: Bland-from-step-zero is the
                // safe default but crawls on degenerate models (scsd1's warm
                // phase II hit the 10000-iteration limit without it); the
                // adaptive switch still falls back to Bland after 20
                // degenerate steps, so cycling remains bounded.
                ref_options.bland_anti_cycling = false;
                // The degenerate optimal face of models like scsd1 can need
                // far more than the default 10000 pivots under Bland's rule.
                ref_options.iteration_limit = 200000;
                const auto warm_res =
                    lp::reference::solve(sparse_working, ref_options, *candidate);
                if (warm_res.status == lp::reference::SolveStatus::optimal &&
                    warm_res.primal.size() == n && warm_res.dual.size() == m) {
                    lp::reference::Result vertex_scaled;
                    vertex_scaled.status = lp::reference::SolveStatus::optimal;
                    vertex_scaled.primal = warm_res.primal;
                    vertex_scaled.dual = warm_res.dual;
                    scale::unscale_solution(scalers, vertex_scaled);
                    out.status = lp::reference::SolveStatus::optimal;
                    out.primal = std::move(vertex_scaled.primal);
                    out.dual = std::move(vertex_scaled.dual);
                    out.objective = model.objective_offset;
                    for (std::size_t j = 0; j < n; ++j)
                        out.objective += model.objective[j] * out.primal[j];
                    out.crossover_applied = true;
                    out.message = "crossover vertex (reference polish)";
                    if (warm_res.condition_estimate > 0.0)
                        out.condition_estimate = warm_res.condition_estimate;
                    try {
                        out.basis_state = lp::dual::make_basis_state(model, warm_res.basis);
                    } catch (const std::exception&) {
                        out.basis_state.reset();
                    }
                }
            } catch (const std::exception&) {
                // Keep the interior point; the engine witness gate decides.
            }
        }
    }
    return out;
}

} // namespace markov_cero::lp::interior
