#pragma once

#include <chrono>
#include <optional>

namespace markov_cero::core {

/// One absolute steady-clock deadline for a whole solve (D06).
///
/// Preparation, search, verification and output share this single deadline so
/// a stage cannot silently receive its own fresh budget. The deadline is
/// cooperative: stages poll it at documented checkpoints. Indivisible work may
/// overrun it, and that overrun must be reported rather than hidden; this is
/// not a real-time guarantee.
class Deadline final {
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    Deadline() = default;
    explicit Deadline(TimePoint at) : at_(at) {}

    [[nodiscard]] static Deadline in(std::chrono::nanoseconds delay) {
        return Deadline(Clock::now() + delay);
    }

    [[nodiscard]] bool has_deadline() const noexcept { return at_.has_value(); }

    [[nodiscard]] bool expired() const noexcept {
        return at_.has_value() && Clock::now() >= *at_;
    }

    /// Remaining time; empty when no deadline is set, zero once expired.
    [[nodiscard]] std::optional<Clock::duration> remaining() const noexcept {
        if (!at_) return std::nullopt;
        const auto left = *at_ - Clock::now();
        return left < Clock::duration::zero() ? Clock::duration::zero() : left;
    }

    [[nodiscard]] std::optional<TimePoint> time_point() const noexcept { return at_; }

    /// Intersection with an earlier deadline (an already-set deadline is
    /// only ever shortened, never extended).
    void shorten_to(TimePoint earlier) noexcept {
        if (!at_ || earlier < *at_) at_ = earlier;
    }

    void shorten_to(const Deadline& other) noexcept {
        if (other.at_) shorten_to(*other.at_);
    }

    /// Clears the deadline. Reserved for tests and for callers that never
    /// start a solve; production paths only ever shorten.
    void clear() noexcept { at_.reset(); }

  private:
    std::optional<TimePoint> at_;
};

} // namespace markov_cero::core
