#pragma once

#include <atomic>
#include <cstddef>
#include <limits>
#include <optional>

namespace markov_cero::core {

/// One solve-wide allocation budget (D06, IR-21).
///
/// The single budget covers factor fill, node queues, cuts, proof data and
/// retained results, so no stage can consume memory the solve does not have.
/// It accounts solver-owned bytes only: caller-owned model bytes stay outside
/// it and are reported separately.
///
/// Contract:
/// - `try_charge` is a preflight: it refuses before any arithmetic can wrap,
///   so an absurd request can never be admitted by overflow.
/// - Exhaustion is sticky. Once a charge is refused the budget fails closed
///   for the rest of the solve, even after releases, so the termination
///   reason stays deterministic and reproducible.
/// - `release` floors at zero; a mis-balanced release can under-count but can
///   never manufacture budget.
/// - Thread-safe: parallel workers charge and release concurrently.
class MemoryBudget final {
  public:
    MemoryBudget() = default;
    explicit MemoryBudget(std::size_t limit_bytes)
        : limit_(limit_bytes), has_limit_(true) {}

    /// Preflight charge. Returns false (and marks the budget exhausted) when
    /// the charge would exceed the limit or overflow the counter.
    [[nodiscard]] bool try_charge(std::size_t bytes) noexcept {
        if (exhausted_.load(std::memory_order_relaxed)) return false;
        std::size_t charged = charged_.load(std::memory_order_relaxed);
        for (;;) {
            if (refuse(charged, bytes)) return false;
            const std::size_t next = charged + bytes;
            if (charged_.compare_exchange_weak(charged, next,
                                               std::memory_order_relaxed)) {
                bump_high_water(next);
                return true;
            }
            // `charged` now holds the observed value; re-check against it.
        }
    }

    void release(std::size_t bytes) noexcept {
        std::size_t charged = charged_.load(std::memory_order_relaxed);
        while (!charged_.compare_exchange_weak(
            charged, charged > bytes ? charged - bytes : 0U,
            std::memory_order_relaxed)) {
        }
    }

    /// Records exhaustion without a charge attempt (an engine that already
    /// knows its budget is gone still gets the same deterministic stop).
    void mark_exhausted() noexcept { exhausted_.store(true, std::memory_order_relaxed); }

    [[nodiscard]] std::size_t charged() const noexcept {
        return charged_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::size_t high_water() const noexcept {
        return high_water_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool exhausted() const noexcept {
        return exhausted_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool unlimited() const noexcept { return !has_limit_; }
    [[nodiscard]] std::optional<std::size_t> limit() const noexcept {
        return has_limit_ ? std::optional<std::size_t>(limit_) : std::nullopt;
    }
    /// Bytes still available before the limit; empty when unlimited, zero
    /// once exhausted.
    [[nodiscard]] std::optional<std::size_t> available() const noexcept {
        if (!has_limit_) return std::nullopt;
        if (exhausted()) return std::size_t{0};
        const std::size_t charged = this->charged();
        return charged >= limit_ ? std::size_t{0} : limit_ - charged;
    }

  private:
    std::atomic<std::size_t> charged_{0};
    std::atomic<std::size_t> high_water_{0};
    std::size_t limit_{0};
    bool has_limit_{false};
    std::atomic<bool> exhausted_{false};

    /// Overflow-safe admission test against one observed charge level.
    [[nodiscard]] bool refuse(std::size_t charged, std::size_t bytes) noexcept {
        if (bytes > std::numeric_limits<std::size_t>::max() - charged) {
            exhausted_.store(true, std::memory_order_relaxed);
            return true;
        }
        if (has_limit_ && (charged > limit_ || limit_ - charged < bytes)) {
            exhausted_.store(true, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    void bump_high_water(std::size_t candidate) noexcept {
        std::size_t observed = high_water_.load(std::memory_order_relaxed);
        while (observed < candidate &&
               !high_water_.compare_exchange_weak(observed, candidate,
                                                  std::memory_order_relaxed)) {
        }
    }
};

} // namespace markov_cero::core
