#pragma once
#include "markov_cero/api/solve.hpp"
#include "markov_cero/core/instrumentation.hpp"
#include "markov_cero/core/solve_context.hpp"
#include "markov_cero/verify/linear_certificate.hpp"

#include "markov_cero/io/lp_parser.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/minlp/minlp_solver.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/interior/ipm.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/milp/parallel_tree_search.hpp"
#include "markov_cero/presolve/presolve.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/model.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/scale/ruiz_scaling.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>


namespace markov_cero::api::detail {
using Clock = std::chrono::steady_clock;
void run_engine(const model::Model& model, const SolveOptions& options, SolveResult& out,
                lp::reference::Result& result, core::SolveContext& ctx);
void run_nonlinear(const model::Model& model, const SolveOptions& options, SolveResult& out,
                   lp::reference::Result& result, core::SolveContext& ctx);
void run_parallel(const model::Model& model, const SolveOptions& options, SolveResult& out,
                  lp::reference::Result& result, core::SolveContext& ctx);
milp::ParallelOptions make_parallel_options(const SolveOptions& options,
                                            core::SolveContext& ctx);
void run_pdlp(const model::Model& model, const SolveOptions& options, SolveResult& out,
              lp::reference::Result& result, core::SolveContext& ctx);
void run_qp(const model::Model& model, const SolveOptions& options, SolveResult& out,
            lp::reference::Result& result, core::SolveContext& ctx);
void certify_mip(const model::Model&, const SolveOptions&, SolveResult&,
                 lp::reference::Result&, core::SolveContext& ctx,
                 const std::vector<verify::MipObligation>& obligations = {});
void run_milp(const model::Model& model, const SolveOptions& options, SolveResult& out,
              lp::reference::Result& result, core::SolveContext& ctx);
void run_lp(const model::Model& model, const SolveOptions& options, SolveResult& out,
            lp::reference::Result& result, core::SolveContext& ctx);
std::string format_violation(const verify::PrimalVerificationReport& report);
void fill_complementarity_gap(NumericalDiagnostic&, const std::vector<double>&,
                              const std::vector<double>&, const std::vector<double>&,
                              const std::vector<double>&);

/// Contract v1 (docs/contracts/numerical-policy.md section 2): the strongest
/// check that actually passed. Derived from status, the verification flags,
/// certificate_type and guarantee_tier; called exactly once by finalize()
/// after the resource-stop invariant has been applied. Engines never set it.
std::string derive_assurance(const SolveResult& out);

/// Result-boundary completion (src/api/finalize.cpp): fills diagnostics,
/// applies the recorded resource stop (resource contract section 3) and
/// derives `assurance` last, exactly once per solve.
void finalize(SolveResult& out, core::SolveContext& ctx);

/// Earliest solve-wide instant implied by the caller's options, measured from
/// API entry: an already-set engine deadline, the LP wall-clock limit and the
/// new solve-wide total limit all intersect here. The MILP duration limit is
/// deliberately excluded because it applies only once an integer model is
/// dispatched (that intersection happens in dispatch).
inline std::optional<Clock::time_point> earliest_deadline(const SolveOptions& options,
                                                          Clock::time_point started) {
    std::optional<Clock::time_point> deadline = options.lp_options.deadline;
    const auto include = [&](std::optional<Clock::time_point> candidate) {
        if (candidate && (!deadline || *candidate < *deadline)) deadline = candidate;
    };
    const auto add_limit = [&](double seconds) {
        if (!std::isfinite(seconds) || seconds <= 0) return;
        const auto remaining = Clock::time_point::max() - started;
        if (seconds < std::chrono::duration<double>(remaining).count())
            include(started + std::chrono::duration_cast<Clock::duration>(
                                  std::chrono::duration<double>(seconds)));
    };
    include(options.milp_options.deadline);
    add_limit(options.lp_options.time_limit_seconds);
    if (options.total_time_limit_seconds) add_limit(*options.total_time_limit_seconds);
    return deadline;
}

inline bool stop_after_deadline(core::SolveContext& ctx, const SolveOptions& options,
                                SolveResult& out, lp::reference::Result& result,
                                const char* phase) {
    (void)options;
    const auto reason = ctx.poll();
    if (reason == core::StopReason::none) return false;
    result.status = lp::reference::SolveStatus::resource_limit;
    result.message = std::string("solve stopped after ") + phase + ": " +
                     core::to_string(reason);
    out.diagnostic.failure_site = reason == core::StopReason::memory_budget_exhausted
                                      ? "memory_budget"
                                  : reason == core::StopReason::deadline_exceeded
                                      ? "api_wall_clock_deadline"
                                      : "solve_stopped";
    out.diagnostic.suggested_recovery = reason == core::StopReason::memory_budget_exhausted
        ? "raise_memory_limit_bytes_or_reduce_the_model"
        : reason == core::StopReason::deadline_exceeded
              ? "increase_time_limit_or_reduce_preprocessing_cost"
              : "inspect_stop_reason_and_engine_message";
    return true;
}

/// Solve-wide resource-option validation at the API boundary (W02). Returns a
/// static description of the first invalid option, or nullptr when the options
/// may be used to build a SolveContext.
inline const char* resource_option_error(const SolveOptions& options) {
    if (options.total_time_limit_seconds) {
        const double seconds = *options.total_time_limit_seconds;
        if (!std::isfinite(seconds) || seconds <= 0.0 || seconds > 1e8)
            return "total_time_limit_seconds must be in (0, 1e8]";
    }
    if (options.memory_limit_bytes && *options.memory_limit_bytes == 0)
        return "memory_limit_bytes must be greater than 0";
    return nullptr;
}

inline core::SolveContext::Config solve_context_config(const SolveOptions& options,
                                                       Clock::time_point started) {
    core::SolveContext::Config config;
    if (const auto deadline = earliest_deadline(options, started))
        config.deadline = core::Deadline(*deadline);
    config.memory_limit_bytes = options.memory_limit_bytes;
    config.thread_quota = options.num_threads;
    config.seed = 0;
    config.deterministic = true;
    config.capabilities.allow_gpu = options.backend == "gpu";
    config.capabilities.allow_ml =
        options.milp_options.branching_strategy == milp::BranchingStrategy::ml_gnn;
    config.capabilities.allow_parallel = options.num_threads > 1;
    config.capabilities.log_model_data = false;
    return config;
}

/// Allocation failure at the API boundary (W02, backlog item 3). Host OOM is
/// a resource outcome, never a numerical failure and never an infeasible or
/// optimal result, and it must not escape `solve_file`/`solve_model` as an
/// exception the caller (CLI, Python, binding) has to survive. The boundary
/// stop (`R3`, contract section 3) is recorded on the shared context so the
/// result explains itself.
inline void fail_allocation(SolveResult& out, lp::reference::Result& result,
                            core::SolveContext& ctx) {
    (void)ctx.note_stop(core::StopReason::allocation_failure);
    result.status = lp::reference::SolveStatus::resource_limit;
    result.message = "allocation failed while preparing or running the solve";
    out.status = result.status;
    out.message = result.message;
    out.error = result.message;
    out.diagnostic.failure_site = "allocation_failure";
    out.diagnostic.suggested_recovery = "reduce_model_size_or_raise_the_host_memory_limit";
}

/// Solve-wide byte charge with failure semantics attached (W02, IR-21). A
/// refused charge records the shared memory-budget stop and reports a
/// resource limit; the solve fails closed rather than running past its budget.
[[nodiscard]] inline bool charge_or_fail(core::SolveContext& ctx, std::size_t bytes,
                                         const char* site, SolveResult& out,
                                         lp::reference::Result& result) {
    if (ctx.charge_or_stop(bytes)) return true;
    result.status = lp::reference::SolveStatus::resource_limit;
    result.message = std::string("solve memory budget exhausted at ") + site;
    out.status = result.status;
    out.message = result.message;
    out.error = result.message;
    out.diagnostic.failure_site = "memory_budget";
    out.diagnostic.suggested_recovery = "raise_memory_limit_bytes_or_reduce_the_model";
    return false;
}
} // namespace markov_cero::api::detail
