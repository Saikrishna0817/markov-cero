#pragma once

#include <cstdint>

namespace markov_cero::core {

/// First-observed cooperative stop condition for one solve (D06/W02).
///
/// Failure semantics shared by every engine, verifier and proof stage:
///
/// - Reasons are sticky. The first observed reason wins so a later reason
///   cannot overwrite the explanation of why work stopped.
/// - Every reason below terminates the solve as a resource/cooperation
///   outcome. Engines report it as `SolveStatus::resource_limit` (or the
///   timeout/limit status a stage already uses) and never as `optimal`,
///   `infeasible` or `unbounded`.
/// - `StopReason::none` means no cooperative stop was recorded; it does not
///   assert that the result is optimal, only that nothing stopped the solve.
/// - Witness completeness is a separate question (W03): a stopped solve must
///   not upgrade an incomplete certificate to a verified one.
///
/// This header stays free of engine/result dependencies so `core` remains
/// below every engine in the dependency order
/// (`model -> algebra -> transforms -> engines -> result interfaces`).
enum class StopReason : std::uint8_t {
    none = 0,
    deadline_exceeded,
    cancelled,
    memory_budget_exhausted,
    queue_capacity_exhausted,
    quota_exhausted,
};

[[nodiscard]] constexpr const char* to_string(StopReason reason) noexcept {
    switch (reason) {
        case StopReason::none: return "none";
        case StopReason::deadline_exceeded: return "deadline_exceeded";
        case StopReason::cancelled: return "cancelled";
        case StopReason::memory_budget_exhausted: return "memory_budget_exhausted";
        case StopReason::queue_capacity_exhausted: return "queue_capacity_exhausted";
        case StopReason::quota_exhausted: return "quota_exhausted";
    }
    return "unknown";
}

/// True when the solve stopped for a resource/cooperation reason and the
/// result must carry a non-success termination status.
[[nodiscard]] constexpr bool is_resource_stop(StopReason reason) noexcept {
    return reason != StopReason::none;
}

} // namespace markov_cero::core
