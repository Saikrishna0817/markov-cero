#pragma once

#include "markov_cero/core/deadline.hpp"
#include "markov_cero/core/memory_budget.hpp"
#include "markov_cero/core/stop_reason.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace markov_cero::core {

/// Opt-in acceleration/backends for one solve (D13/D14/D25).
///
/// Nothing here is enabled by default: GPU and ML activation stay explicit and
/// evidence-gated, and each capability is reported rather than assumed.
struct CapabilityPolicy final {
    bool allow_gpu{false};
    bool allow_ml{false};
    bool allow_parallel{true};
    /// D25: model coefficient/name logging is opt-in. Metrics (stage timings,
    /// counters, memory high-water) are always local and safe to trace.
    bool log_model_data{false};
};

/// One structured, caller-controlled trace record (D25).
///
/// Stage names are borrowed static strings ("presolve", "node_lp", ...);
/// `reason` carries the stop condition when the event explains a termination.
struct TraceEvent final {
    const char* stage{""};
    StopReason reason{StopReason::none};
    std::size_t count{0};
    std::size_t bytes{0};
    double elapsed_ms{0.0};
    /// Origin: 0 for solve-level stages, the worker index otherwise
    /// (W02 worker isolation). Added last so aggregate initializers of the
    /// older fields keep their meaning.
    std::uint32_t worker{0};
};

/// Caller-owned trace sink. Emission is best-effort and must never throw;
/// the solve does not depend on observability succeeding. Implementations may
/// be called concurrently from worker threads.
class TraceSink {
  public:
    virtual ~TraceSink() = default;
    virtual void emit(const TraceEvent& event) noexcept = 0;
};

/// Per-solve resource and capability context (D06/D25, W01/W02).
///
/// Ownership: one context is created per solve by the API boundary, owned by
/// the caller of the engine, and must outlive every stage it is passed to.
/// The trace sink is borrowed and must outlive the context. No globals: each
/// solve carries its own deadline, budget, quotas, seed and policy.
///
/// Threading: `deadline` and `memory` are internally synchronized or
/// read-only after construction; `request_cancel`, `note_stop`, `poll` and
/// `charge_or_stop` are safe to call from any worker. Trace emission may be
/// concurrent, so sinks must synchronize themselves.
///
/// Narrow interfaces: stages should take `const Deadline&`, `MemoryBudget&`
/// or a capability flag rather than the whole context, so no module needs
/// arbitrary access to everything at once.
class SolveContext final {
  public:
    struct Config final {
        Deadline deadline;
        std::optional<std::size_t> memory_limit_bytes;
        std::size_t thread_quota{0}; // 0 = unlimited
        std::size_t device_quota{0}; // 0 = unlimited
        std::uint64_t seed{0};
        bool deterministic{true};
        CapabilityPolicy capabilities{};
    };

    SolveContext() = default;
    explicit SolveContext(const Config& config)
        : deadline_(config.deadline),
          memory_(config.memory_limit_bytes
                      ? MemoryBudget(*config.memory_limit_bytes)
                      : MemoryBudget()),
          thread_quota_(config.thread_quota),
          device_quota_(config.device_quota),
          seed_(config.seed),
          deterministic_(config.deterministic),
          capabilities_(config.capabilities) {}

    [[nodiscard]] const Deadline& deadline() const noexcept { return deadline_; }
    [[nodiscard]] Deadline& deadline() noexcept { return deadline_; }
    [[nodiscard]] const MemoryBudget& memory() const noexcept { return memory_; }
    [[nodiscard]] MemoryBudget& memory() noexcept { return memory_; }

    [[nodiscard]] std::size_t thread_quota() const noexcept { return thread_quota_; }
    [[nodiscard]] std::size_t device_quota() const noexcept { return device_quota_; }
    [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
    [[nodiscard]] bool deterministic() const noexcept { return deterministic_; }
    [[nodiscard]] const CapabilityPolicy& capabilities() const noexcept {
        return capabilities_;
    }

    /// Cooperative cancellation (D06): distinct from the deadline and from any
    /// process-enforced service limit. Safe to call from any thread.
    void request_cancel() noexcept { cancelled_.store(true, std::memory_order_release); }
    [[nodiscard]] bool cancel_requested() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    /// Sticky first stop reason. Returns the reason this call recorded, or the
    /// reason already recorded by an earlier call.
    [[nodiscard]] StopReason note_stop(StopReason reason) noexcept {
        if (reason == StopReason::none) return stop_reason();
        StopReason expected = StopReason::none;
        stopped_.compare_exchange_strong(expected, reason, std::memory_order_acq_rel);
        return stopped_.load(std::memory_order_acquire);
    }

    /// Polls cancellation and the deadline, records the first stop, returns
    /// the sticky reason.
    [[nodiscard]] StopReason poll() noexcept {
        if (cancelled_.load(std::memory_order_acquire))
            return note_stop(StopReason::cancelled);
        if (deadline_.expired()) return note_stop(StopReason::deadline_exceeded);
        return stop_reason();
    }

    /// Deadline instrumentation: milliseconds left before the shared deadline
    /// (0.0 once expired), empty when the solve carries no deadline. Stages
    /// use it to report slack; enforcement stays cooperative through `poll`.
    [[nodiscard]] std::optional<double> remaining_ms() const noexcept {
        const auto left = deadline_.remaining();
        if (!left) return std::nullopt;
        return std::chrono::duration<double, std::milli>(*left).count();
    }

    [[nodiscard]] StopReason stop_reason() const noexcept {
        return stopped_.load(std::memory_order_acquire);
    }

    /// True once a stop has been recorded. Poll first: an unpolled expired
    /// deadline is not yet a recorded stop.
    [[nodiscard]] bool stopped() const noexcept {
        return stop_reason() != StopReason::none;
    }

    /// Budget admission with failure semantics attached: a refused charge
    /// records `memory_budget_exhausted` so the stage that ran out of memory
    /// reports the reason instead of inventing a solver status.
    [[nodiscard]] bool charge_or_stop(std::size_t bytes) noexcept {
        if (memory_.try_charge(bytes)) return true;
        [[maybe_unused]] const StopReason recorded =
            note_stop(StopReason::memory_budget_exhausted);
        return false;
    }

    /// Borrowed sink; must outlive this context. Pass nullptr to disable.
    void set_trace_sink(TraceSink* sink) noexcept { sink_ = sink; }
    [[nodiscard]] TraceSink* trace_sink() const noexcept { return sink_; }

    void emit(const TraceEvent& event) noexcept {
        if (sink_) sink_->emit(event);
    }

  private:
    Deadline deadline_{};
    MemoryBudget memory_{};
    std::size_t thread_quota_{0};
    std::size_t device_quota_{0};
    std::uint64_t seed_{0};
    bool deterministic_{true};
    CapabilityPolicy capabilities_{};
    std::atomic<bool> cancelled_{false};
    std::atomic<StopReason> stopped_{StopReason::none};
    TraceSink* sink_{nullptr};
};

} // namespace markov_cero::core
