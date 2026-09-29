#include "markov_cero/qp/admm_solver.hpp"
#include <algorithm>
#include <cmath>
#include "admm_solver_internal.hpp"

namespace markov_cero::qp {
QpSolution solve_qp(const QuadraticModel& model, const QpOptions& options) {
    return solve_qp(model, options, false);
}
QpSolution solve_qp(const QuadraticModel& model, const QpOptions& options, bool gpu) {
    AdmmQpSolver fresh_session;
    return solve_qp(model, options, fresh_session, gpu);
}
QpSolution solve_qp(const QuadraticModel& model, const QpOptions& options,
                    AdmmQpSolver& session, bool gpu) {
    const auto started = std::chrono::steady_clock::now();
    auto settings = options;
    if (std::isfinite(options.time_limit_seconds) && options.time_limit_seconds >= 0 && options.time_limit_seconds < 1e8) {
        const auto end = started + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(options.time_limit_seconds));
        if (!settings.deadline || end < *settings.deadline) settings.deadline = end;
    }
    auto scaled = model;
    if (!scaled.P.dimension && scaled.P.column_offsets.empty()) scaled.P.column_offsets = {0};
    if (!scaled.A.columns && scaled.A.column_offsets.empty()) scaled.A.column_offsets = {0};
    scaled.validate();
    const auto n = model.num_variables(), m = model.num_constraints();
    std::vector<double> d(n, 1), e(m, 1);
    for (int pass = 0; pass < 8; ++pass) {
        if (settings.deadline && std::chrono::steady_clock::now() >= *settings.deadline) {
            QpSolution out; out.status = QpStatus::time_limit;
            out.message = "deadline reached during QP equilibration"; return out;
        }
        std::vector<double> cn(n, 0), rn(m, 0);
        for (std::size_t j = 0; j < n; ++j) {
            for (auto k = scaled.P.column_offsets[j]; k < scaled.P.column_offsets[j + 1]; ++k) {
                cn[j] = std::max(cn[j], std::abs(scaled.P.values[k]));
                cn[scaled.P.row_indices[k]] = std::max(cn[scaled.P.row_indices[k]], std::abs(scaled.P.values[k]));
            }
            for (auto k = scaled.A.column_offsets[j]; k < scaled.A.column_offsets[j + 1]; ++k) {
                cn[j] = std::max(cn[j], std::abs(scaled.A.values[k]));
                rn[scaled.A.row_indices[k]] = std::max(rn[scaled.A.row_indices[k]], std::abs(scaled.A.values[k]));
            }
        }
        for (auto& x : cn) x = x == 0 ? 1 : 1 / std::sqrt(std::clamp(x, 1e-4, 1e4));
        for (auto& x : rn) x = x == 0 ? 1 : 1 / std::sqrt(std::clamp(x, 1e-4, 1e4));
        for (std::size_t j = 0; j < n; ++j) {
            d[j] *= cn[j]; scaled.q[j] *= cn[j];
            for (auto k = scaled.P.column_offsets[j]; k < scaled.P.column_offsets[j + 1]; ++k)
                scaled.P.values[k] *= cn[j] * cn[scaled.P.row_indices[k]];
            for (auto k = scaled.A.column_offsets[j]; k < scaled.A.column_offsets[j + 1]; ++k)
                scaled.A.values[k] *= cn[j] * rn[scaled.A.row_indices[k]];
        }
        for (std::size_t i = 0; i < m; ++i) {
            e[i] *= rn[i]; scaled.l[i] *= rn[i]; scaled.u[i] *= rn[i];
        }
    }
    // Scaling is algebraic only; all returned witnesses use the caller's units.
    // The caller-owned session keeps the KKT symbolic pattern cache across
    // repeated solves; a fresh session in the 3-arg overloads starts empty, so
    // one-shot callers keep the previous per-solve behavior.
    session.set_options(settings);
    session.set_gpu_backend(gpu);
    auto result = session.solve(scaled);
    for (std::size_t j = 0; j < result.x.size(); ++j) result.x[j] *= d[j];
    for (std::size_t j = 0; j < result.unbounded_ray.size(); ++j) result.unbounded_ray[j] *= d[j];
    for (std::size_t i = 0; i < result.y.size(); ++i) result.y[i] *= e[i];
    for (std::size_t i = 0; i < result.z.size(); ++i) result.z[i] /= e[i];
    for (std::size_t i = 0; i < result.infeasibility_certificate.size(); ++i)
        result.infeasibility_certificate[i] *= e[i];
    if (result.x.size() == n) {
        long double objective = .5L * model.P.evaluate_energy(result.x);
        for (std::size_t j = 0; j < n; ++j) objective += static_cast<long double>(model.q[j]) * result.x[j];
        result.objective_value = model.objective_offset +
            (model.sense == model::ObjectiveSense::maximize ? -objective : objective);
    }
    if (result.x.size() == n && result.y.size() == m && result.z.size() == m) {
        const auto ax = detail_admm_solver::multiply_A(model.A, result.x);
        const auto aty = detail_admm_solver::multiply_AT(model.A, result.y);
        const auto px = model.P.multiply(result.x);
        result.primal_residual = result.dual_residual = 0;
        for (std::size_t i=0; i<m; ++i)
            result.primal_residual = std::max(result.primal_residual, std::abs(ax[i]-result.z[i]));
        for (std::size_t j=0; j<n; ++j)
            result.dual_residual = std::max(result.dual_residual, std::abs(px[j]+model.q[j]+aty[j]));
    }
    // Equilibration changes units; certify the final caller-facing witness too.
    detail_admm_solver::verify_accepted_result(model, result);
    result.solve_time_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    return result;
}
} // namespace markov_cero::qp
