#include "api_internal.hpp"

#include <fstream>
#include <string>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace markov_cero::api::detail {
namespace {
/// IR-21 (contract §4): one sampling of the process resident-set high-water
/// mark. Linux exposes VmHWM directly; other POSIX hosts fall back to
/// getrusage (bytes on macOS); everything else reports 0 = unavailable.
std::size_t peak_rss_bytes() noexcept {
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string key;
    while (status >> key) {
        if (key == "VmHWM:") {
            std::size_t kib = 0;
            status >> kib;
            return kib * 1024U;
        }
        std::string rest;
        std::getline(status, rest);
    }
    return 0;
#elif defined(__APPLE__)
    struct rusage usage {};
    return getrusage(RUSAGE_SELF, &usage) == 0
               ? static_cast<std::size_t>(usage.ru_maxrss)
               : std::size_t{0};
#else
    return 0;
#endif
}

/// IR-21: retained result vectors admitted as solver-owned bytes before the
/// high-water mark is published (contract §4). Best-effort: a refused
/// post-solve charge never records a stop or converts a completed result —
/// it only stops adding bytes, leaving the stop reasons below untouched.
void charge_retained_results(SolveResult& out, core::SolveContext& ctx) noexcept {
    const std::size_t doubles = out.primal.size() + out.original_primal.size() +
                                out.row_activities.size() + out.row_lower_slacks.size() +
                                out.row_upper_slacks.size() + out.row_duals.size() +
                                out.reduced_costs.size();
    (void)ctx.memory().try_charge(doubles * sizeof(double));
}
} // namespace
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
    if (reason == core::StopReason::memory_budget_exhausted && !out.message.empty()) {
        // Keep the site-specific charge message ("... exhausted at
        // working_model", factor/KKT budget refusals): it names where the
        // budget ran out, which the generic sentence below would erase.
        out.error = out.message;
    } else {
        out.message = "solve stopped (" + out.stop_reason +
                      ") before the result could be verified";
        out.error = out.message;
    }
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
    // high-water mark of admitted solver-owned charges, after the retained
    // result vectors join the ledger.
    charge_retained_results(out, ctx);
    out.memory_charged_peak_bytes = ctx.memory().high_water();
    // IR-21 (contract section 4): process resident-set high-water sampled at
    // finalization — a measurement, never an enforced ceiling.
    out.peak_rss_bytes = peak_rss_bytes();
    apply_resource_stop(out, ctx);
    // Derived last so the label reflects the post-stop status and flags: a
    // resource stop downgrades the status before the label is computed.
    out.assurance = derive_assurance(out);
}

} // namespace markov_cero::api::detail
