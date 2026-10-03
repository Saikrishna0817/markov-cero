#pragma once

#include <cstdint>

namespace markov_cero::core {

/// First-observed resource stop for one solve (D06/W02). Contract:
/// docs/contracts/resource-limits.md.
///
/// Failure semantics shared by every engine, verifier and proof stage:
///
/// - Reasons are sticky. The first observed reason wins so a later reason
///   cannot overwrite the explanation of why work stopped.
/// - Every reason below terminates the solve as a resource/cooperation
///   outcome. Engines report it as `SolveStatus::resource_limit` (or the
///   timeout/limit status a stage already uses) and never as `optimal`,
///   `infeasible` or `unbounded`.
/// - Cooperative stops (deadline, cancellation, budget, quota) are recorded by
///   the shared SolveContext from inside the solve. Boundary stops
///   (`input_limit`, `work_limit`, `allocation_failure`) are recorded by the
///   API boundary when it maps a limit hit or a host allocation failure to a
///   resource outcome.
/// - `StopReason::none` means no stop was recorded; it does not assert that
///   the result is optimal, only that nothing stopped the solve.
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
    // API-boundary attributions (R3): appended so existing values keep their
    // numeric identity.
    input_limit,
    work_limit,
    allocation_failure,
    device_memory_budget_exhausted,
};

[[nodiscard]] constexpr const char* to_string(StopReason reason) noexcept {
    switch (reason) {
        case StopReason::none: return "none";
        case StopReason::deadline_exceeded: return "deadline_exceeded";
        case StopReason::cancelled: return "cancelled";
        case StopReason::memory_budget_exhausted: return "memory_budget_exhausted";
        case StopReason::queue_capacity_exhausted: return "queue_capacity_exhausted";
        case StopReason::quota_exhausted: return "quota_exhausted";
        case StopReason::input_limit: return "input_limit";
        case StopReason::work_limit: return "work_limit";
        case StopReason::allocation_failure: return "allocation_failure";
        case StopReason::device_memory_budget_exhausted: return "device_memory_budget_exhausted";
    }
    return "unknown";
}

/// True when the solve stopped for a resource/cooperation reason and the
/// result must carry a non-success termination status.
[[nodiscard]] constexpr bool is_resource_stop(StopReason reason) noexcept {
    return reason != StopReason::none;
}

} // namespace markov_cero::core
