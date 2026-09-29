#pragma once

#include "markov_cero/core/solve_context.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace markov_cero::core {

/// Worker isolation boundary for one solve (W02 / backlog item 3).
///
/// One `WorkerContext` wraps the shared `SolveContext` for exactly one search
/// worker. It is the contract that says what a worker may touch and what it
/// must not.
///
/// Shared by every worker (one synchronized copy on the `SolveContext`):
/// - the absolute deadline and cooperative cancellation,
/// - the single `MemoryBudget` (one accounting for the whole solve, so no
///   worker can spend memory the solve does not have),
/// - the sticky solve-wide stop reason, capability policy, seed and quotas,
/// - the caller-owned trace sink.
///
/// Private to one worker:
/// - charge attribution: `charged_by_worker()` counts only this worker's live
///   bytes, so peak accounting can separate worker allocations from root and
///   queue storage;
/// - release rights: `release()` can never return more than this worker
///   charged, so one worker cannot under-count another's live bytes;
/// - a local sticky stop reason: a worker that hits its own limit (a local
///   queue or fill cap) stops itself without stopping its siblings, and the
///   shared context is not polluted with a reason that belongs to one worker.
///
/// Failure semantics:
/// - A refused charge marks the *shared* budget exhausted and records the
///   solve-wide `memory_budget_exhausted` stop, because the solve as a whole
///   is out of memory; it also records the same reason locally, so the worker
///   that made the failing request stops immediately.
/// - `poll()` reports a shared stop first (a worker must obey a solve-wide
///   stop), then the worker's local stop, then cancellation/deadline as
///   observed through the shared context.
/// - Destruction releases whatever this worker still holds, so an early exit
///   or exception cannot strand charged bytes.
///
/// Threading: `charge`, `release` and `note_local_stop` are called by the
/// owning worker only (`held_` is single-writer). The local stop reason is an
/// atomic so a driver may read it concurrently. The destructor must run after
/// the worker has finished (join provides the happens-before edge); it is
/// then safe on any thread. Shared state is synchronized inside
/// `SolveContext`/`MemoryBudget`.
class WorkerContext final {
  public:
    WorkerContext(SolveContext& shared, std::size_t index) noexcept
        : shared_(&shared), index_(index) {}

    ~WorkerContext() { release(held_); }

    WorkerContext(const WorkerContext&) = delete;
    WorkerContext& operator=(const WorkerContext&) = delete;
    WorkerContext(WorkerContext&&) = delete;
    WorkerContext& operator=(WorkerContext&&) = delete;

    [[nodiscard]] std::size_t index() const noexcept { return index_; }
    [[nodiscard]] SolveContext& shared() noexcept { return *shared_; }

    /// Preflight charge against the shared budget, attributed to this worker.
    [[nodiscard]] bool charge(std::size_t bytes) noexcept {
        if (bytes > std::numeric_limits<std::size_t>::max() - held_) {
            // Attribution itself would wrap: fail closed for the solve and
            // for this worker rather than tracking a wrapped counter.
            shared_->memory().mark_exhausted();
            [[maybe_unused]] const StopReason shared_recorded =
                shared_->note_stop(StopReason::memory_budget_exhausted);
            [[maybe_unused]] const StopReason local_recorded =
                note_local_stop(StopReason::memory_budget_exhausted);
            return false;
        }
        if (!shared_->charge_or_stop(bytes)) {
            [[maybe_unused]] const StopReason refused =
                note_local_stop(StopReason::memory_budget_exhausted);
            return false;
        }
        held_ += bytes;
        return true;
    }

    /// Returns at most what this worker still holds; extra bytes are dropped
    /// so a mis-balanced release can never manufacture budget for others.
    void release(std::size_t bytes) noexcept {
        const std::size_t give_back = bytes > held_ ? held_ : bytes;
        if (give_back == 0) return;
        held_ -= give_back;
        shared_->memory().release(give_back);
    }

    [[nodiscard]] std::size_t charged_by_worker() const noexcept { return held_; }

    /// Worker-local sticky stop. The first reason wins, and it stops only
    /// this worker until the shared context records a solve-wide stop.
    [[nodiscard]] StopReason note_local_stop(StopReason reason) noexcept {
        if (reason == StopReason::none) return local_stop_reason();
        StopReason expected = StopReason::none;
        local_.compare_exchange_strong(expected, reason, std::memory_order_acq_rel);
        return local_stop_reason();
    }

    [[nodiscard]] StopReason local_stop_reason() const noexcept {
        return local_.load(std::memory_order_acquire);
    }

    /// Solve-wide stop first, then this worker's local stop.
    [[nodiscard]] StopReason poll() noexcept {
        const StopReason shared = shared_->poll();
        if (shared != StopReason::none) return shared;
        return local_stop_reason();
    }

    /// Emit an event tagged with this worker's index on the shared sink.
    void emit(const TraceEvent& event) noexcept {
        TraceEvent tagged = event;
        tagged.worker = static_cast<std::uint32_t>(index_);
        shared_->emit(tagged);
    }

  private:
    SolveContext* shared_;
    std::size_t index_;
    std::size_t held_{0};
    std::atomic<StopReason> local_{StopReason::none};
};

} // namespace markov_cero::core
