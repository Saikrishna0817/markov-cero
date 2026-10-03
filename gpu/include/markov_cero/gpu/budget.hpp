#pragma once

#include <cstddef>
#include <new>
#include <optional>

namespace markov_cero::gpu {

/// Thrown when a solve-scoped device budget refuses a buffer charge. The API
/// boundary maps it to `resource_limit` + `device_memory_budget_exhausted`
/// (docs/contracts/resource-limits.md).
struct DeviceBudgetExhausted : std::bad_alloc {};

/// Thrown when the device allocator itself fails after the budget admitted
/// the charge (cudaMalloc in a CUDA build). The API boundary maps it to the
/// shared `allocation_failure` reason with device-specific diagnostics; a
/// CPU-only build keeps plain `std::bad_alloc` for host malloc failure.
struct DeviceAllocationFailure : std::bad_alloc {};

/// Solve-scoped, thread-local meter for bytes admitted through the gpu
/// buffer layer. Installed by `api::detail::run_engine` when
/// `backend == "gpu"` for the engine phase; `DeviceBuffer` charges on
/// allocation and releases on free, so the counter tracks live-outstanding
/// device bytes. Semantics mirror `core::MemoryBudget`:
///
/// - Unset limit: every charge is accounted and none is ever refused.
/// - Set limit: the first charge that would exceed it marks the budget
///   exhausted (sticky for the rest of the scope) and is refused before the
///   device allocator runs.
/// - The scope's destructor publishes the admitted high-water mark through
///   the peak sink (normal return and unwinding alike) and restores the
///   enclosing scope, so nesting is safe.
///
/// The meter is single-threaded (it runs on the solving thread) and bounds
/// gpu buffer layer bytes only — not CUDA contexts, streams or other
/// processes (contract §4).
class DeviceBudget final {
  public:
    explicit DeviceBudget(std::optional<std::size_t> limit_bytes,
                          std::size_t* peak_sink = nullptr) noexcept;
    ~DeviceBudget();

    DeviceBudget(const DeviceBudget&) = delete;
    DeviceBudget& operator=(const DeviceBudget&) = delete;

    /// The scope installed on the calling thread, or nullptr outside one.
    [[nodiscard]] static DeviceBudget* current() noexcept;

    /// Preflight charge. Returns false (and marks the budget exhausted)
    /// when a limit is set and the charge would exceed it.
    [[nodiscard]] bool try_charge(std::size_t bytes) noexcept;
    void release(std::size_t bytes) noexcept;
    void mark_exhausted() noexcept;

    [[nodiscard]] std::size_t charged() const noexcept { return charged_; }
    [[nodiscard]] std::size_t high_water() const noexcept { return high_water_; }
    [[nodiscard]] bool exhausted() const noexcept { return exhausted_; }
    [[nodiscard]] const std::optional<std::size_t>& limit() const noexcept { return limit_; }

  private:
    /// Overflow-safe admission test for one charge.
    [[nodiscard]] bool refuse(std::size_t bytes) noexcept;

    DeviceBudget* previous_;
    std::size_t* peak_sink_;
    std::optional<std::size_t> limit_;
    std::size_t charged_{0};
    std::size_t high_water_{0};
    bool exhausted_{false};
};

} // namespace markov_cero::gpu
