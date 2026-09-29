#pragma once

#include "markov_cero/core/solve_context.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace markov_cero::core {

/// RAII stage instrumentation for one solve (W02 / D25).
///
/// A `StageScope` times one stage ("presolve", "node_lp", "proof_replay", ...)
/// and emits exactly one `TraceEvent` when it leaves scope, including early
/// returns and exception unwinding. Emission is best-effort: a solve never
/// depends on observability succeeding, so `emit` never throws.
///
/// Event payload:
/// - `stage` is a borrowed static string; it must outlive the scope.
/// - `reason` is the shared sticky stop reason at scope exit (a stage that
///   stopped for a resource limit reports that reason, not `none`).
/// - `count` is whatever the stage chose to count (iterations, nodes, cuts);
///   it starts at zero and is set with `set_count`/`add_count`.
/// - `bytes` is the net change in solver-owned charged bytes during the
///   stage, floored at zero. It is an attribution, not a peak measurement:
///   use `MemoryBudget::high_water()` for the solve-wide peak.
/// - `elapsed_ms` is wall time from construction to scope exit. It measures
///   observed duration; it does not assert that the stage respected the
///   deadline.
/// - `worker` carries the optional worker index so parallel stages are
///   distinguishable; solve-level stages leave it at zero.
///
/// Threading: one scope belongs to the thread that constructed it. The shared
/// context it reports into is internally synchronized, so concurrent emission
/// from several workers is safe as long as each sink synchronizes itself.
class StageScope final {
  public:
    using Clock = std::chrono::steady_clock;

    StageScope(SolveContext& context, const char* stage,
               std::uint32_t worker = 0) noexcept
        : context_(&context),
          stage_(stage),
          started_(Clock::now()),
          charged_at_start_(context.memory().charged()),
          worker_(worker) {}

    ~StageScope() { emit(); }

    StageScope(const StageScope&) = delete;
    StageScope& operator=(const StageScope&) = delete;
    StageScope(StageScope&&) = delete;
    StageScope& operator=(StageScope&&) = delete;

    void set_count(std::size_t count) noexcept { count_ = count; }
    void add_count(std::size_t delta) noexcept { count_ += delta; }

    [[nodiscard]] const char* stage() const noexcept { return stage_; }
    [[nodiscard]] std::size_t count() const noexcept { return count_; }

  private:
    void emit() noexcept {
        const auto ended = Clock::now();
        const std::size_t charged = context_->memory().charged();
        TraceEvent event;
        event.stage = stage_;
        event.reason = context_->stop_reason();
        event.count = count_;
        event.bytes = charged > charged_at_start_ ? charged - charged_at_start_ : 0;
        event.elapsed_ms =
            std::chrono::duration<double, std::milli>(ended - started_).count();
        event.worker = worker_;
        context_->emit(event);
    }

    SolveContext* context_;
    const char* stage_;
    Clock::time_point started_;
    std::size_t charged_at_start_;
    std::uint32_t worker_;
    std::size_t count_{0};
};

} // namespace markov_cero::core
