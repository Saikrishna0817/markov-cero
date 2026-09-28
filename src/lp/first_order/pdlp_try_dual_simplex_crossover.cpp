#include "pdlp_internal.hpp"
namespace markov_cero::lp::first_order {
using namespace detail_pdlp;
namespace detail_pdlp {
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
        dual_options.deadline = options.deadline;
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
            for (std::size_t i = 0; i < canon.record.rows.size(); ++i) {
                const auto& map = canon.record.rows[i];
                for (std::size_t k = 0; k < map.canonical_index.size(); ++k)
                    res.dual[i] -= map.multiplier[k] * dual_res.solution.dual[map.canonical_index[k]];
                if (ruiz_scaling) res.dual[i] *= scalers.row_scale[i];
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
}

}
