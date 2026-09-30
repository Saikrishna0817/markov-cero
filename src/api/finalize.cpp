#include "api_internal.hpp"

namespace markov_cero::api::detail {
/// Reports the recorded resource stop on the result, and enforces the W02
/// invariant that a resource stop never accompanies an unverified optimal,
/// infeasible or unbounded claim.
void apply_resource_stop(SolveResult& out, core::SolveContext& ctx) {
    const core::StopReason reason = ctx.stop_reason();
    if (reason == core::StopReason::none) {
        // R2/R4 (contract section 3): a resource_limit result must always
        // explain itself. When no reason reached the shared context, the
        // boundary knows work stopped for a resource cause but cannot
        // attribute it further, so it says exactly that instead of guessing.
        if (out.status == lp::reference::SolveStatus::resource_limit &&
            out.stop_reason.empty()) {
            out.stop_reason = "unspecified_resource_limit";
        }
        return;
    }
    out.stop_reason = core::to_string(reason);
    if (reason == core::StopReason::memory_budget_exhausted) {
        out.diagnostic.failure_site = "memory_budget";
        out.diagnostic.suggested_recovery = "raise_memory_limit_bytes_or_reduce_the_model";
    }
    const bool unclaimed_success =
        out.status == lp::reference::SolveStatus::optimal ||
        out.status == lp::reference::SolveStatus::infeasible ||
        out.status == lp::reference::SolveStatus::unbounded;
    if (!unclaimed_success && reason != core::StopReason::memory_budget_exhausted) return;
    out.status = lp::reference::SolveStatus::resource_limit;
    out.message = "solve stopped (" + out.stop_reason +
                  ") before the result could be verified";
    out.error = out.message;
    out.verified = false;
}

void finalize(SolveResult& out, core::SolveContext& ctx) {
    out.verified =
        (out.status == lp::reference::SolveStatus::optimal && out.original_verified &&
         out.canonical_verified) ||
        ((out.status == lp::reference::SolveStatus::infeasible ||
          out.status == lp::reference::SolveStatus::unbounded) &&
         out.canonical_verified);

    // Eliminate silent failures: guarantee valid diagnostic state
    if (out.diagnostic.failure_site.empty()) {
        if (out.status == lp::reference::SolveStatus::optimal) {
            out.diagnostic.failure_site = "none";
            out.diagnostic.suggested_recovery = "none";
        } else {
            out.diagnostic.failure_site = out.resolved_engine + "_solve";
            out.diagnostic.suggested_recovery = "inspect_engine_numerics_and_parameters";
        }
    }
    if (out.diagnostic.primal_residual == 0.0) {
        out.diagnostic.primal_residual = std::max(out.canonical_report.maximum_primal_violation,
                                                 out.primal_report.maximum_row_violation);
    }
    if (out.diagnostic.dual_residual == 0.0) {
        out.diagnostic.dual_residual = out.canonical_report.maximum_dual_violation;
    }
    // condition_estimate is engine-populated: 0.0 here means the resolved
    // engine performed no factorization (matrix-free PDLP / SQP), which is
    // reported as-is rather than replaced by a fake "perfectly conditioned".
    // IR-21 peak diagnostic (resource contract section 2): the budget's
    // high-water mark of admitted solver-owned charges.
    out.memory_charged_peak_bytes = ctx.memory().high_water();
    apply_resource_stop(out, ctx);
    // Derived last so the label reflects the post-stop status and flags: a
    // resource stop downgrades the status before the label is computed.
    out.assurance = derive_assurance(out);
}

} // namespace markov_cero::api::detail
