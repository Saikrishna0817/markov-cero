#include "api_internal.hpp"
#include "engine_stages.hpp"

namespace markov_cero::api::detail {
void run_lp_ipm_engine(const transform::SparseCanonicalModel& working_model,
                       const SolveOptions& options, SolveResult& out,
                       lp::reference::Result& result,
                       std::optional<lp::dual::BasisState>& basis_to_save) {
    lp::interior::Options ipm_opts;
    ipm_opts.iteration_limit = 100;
    ipm_opts.deadline = options.lp_options.deadline;
    if (options.lp_options.iteration_limit > 0 &&
        options.lp_options.iteration_limit != 10000) {
        ipm_opts.iteration_limit =
            std::min<std::size_t>(options.lp_options.iteration_limit, 500);
    }
    bool ipm_certified = false;
    std::string ipm_failure;
    lp::interior::Result ipm_res;
    try {
        ipm_res = lp::interior::solve(working_model, ipm_opts);
        ipm_certified =
            ipm_res.status == lp::reference::SolveStatus::optimal;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::length_error&) {
        throw;
    } catch (const std::exception& e) {
        ipm_failure = e.what();
    }
    if (ipm_certified) {
        result.status = lp::reference::SolveStatus::optimal;
        result.primal = ipm_res.primal;
        result.dual = ipm_res.dual;
        result.objective = ipm_res.objective;
        result.message = ipm_res.message;
        result.condition_estimate = ipm_res.condition_estimate;
        out.lp_iterations = ipm_res.iterations;
        if (ipm_res.basis_state.has_value()) {
            basis_to_save = *ipm_res.basis_state;
        }
    } else {
        result = lp::reference::solve(working_model, options.lp_options);
        if (result.status == lp::reference::SolveStatus::optimal &&
            result.basis.size() == working_model.matrix.rows) {
            // Degenerate optimal bases (duplicate indices after
            // the primal engine fixes variables at bounds) cannot
            // seed a warm start; the dual engine's own contract is
            // "keep the solve result, drop the warm start", so the
            // API path mirrors it instead of discarding a verified
            // optimum behind a thrown exception.
            try {
                basis_to_save =
                    lp::dual::make_basis_state(working_model, result.basis);
            } catch (const std::bad_alloc&) {
                throw;
            } catch (const std::length_error&) {
                throw;
            } catch (const std::exception&) {
                basis_to_save.reset();
            }
        }
        result.message =
            (ipm_failure.empty()
                 ? "ipm did not certify (" + ipm_res.message +
                       "); reference primal revised simplex fallback"
                 : "ipm numerical failure (" + ipm_failure +
                       "); reference primal revised simplex fallback");
        out.used_cold_fallback = true;
    }
}

} // namespace markov_cero::api::detail
