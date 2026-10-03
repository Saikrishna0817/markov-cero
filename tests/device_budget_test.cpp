// RES-01 (docs/contracts/resource-limits.md §1/§2/§4): solve-scoped device
// buffer metering — refuse before the allocator, live release keeps churn
// inside the limit, sticky refusal, and the boundary maps a refusal to
// resource_limit + device_memory_budget_exhausted on a real solve.
#include "markov_cero/api/solve.hpp"
#include "markov_cero/gpu/budget.hpp"
#include "markov_cero/gpu/buffer.hpp"
#include "markov_cero/gpu/device.hpp"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>

using namespace markov_cero;

namespace {

void req(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

// Meter semantics (contract §1): admission, release, stickiness, peak sink.
void test_meter_semantics() {
    std::size_t peak = 0;
    {
        gpu::DeviceBudget budget(std::size_t{16}, &peak);
        req(budget.try_charge(10), "a charge under the limit is admitted");
        req(budget.charged() == 10, "the meter tracks admitted bytes");
        req(!budget.try_charge(8), "a charge over the limit is refused");
        req(budget.exhausted(), "the first refusal marks the budget exhausted");
        req(budget.charged() == 10, "a refused charge never lands");
        req(!budget.try_charge(1), "exhaustion is sticky for the rest of the scope");
        budget.release(10);
        req(budget.charged() == 0, "release returns admitted bytes to the meter");
        req(!budget.try_charge(4), "stickiness survives release");
        req(budget.high_water() == 10, "the high-water mark keeps admitted bytes");
    }
    req(peak == 10, "the scope publishes the high-water mark on destruction");
    {
        gpu::DeviceBudget unlimited(std::nullopt, &peak);
        req(unlimited.try_charge(std::size_t{1} << 30),
            "an unset limit accounts but never refuses");
        req(unlimited.charged() == (std::size_t{1} << 30),
            "accounting runs without a limit");
    }
    req(peak == (std::size_t{1} << 30), "an unlimited scope still publishes its peak");
    {
        gpu::DeviceBudget outer(std::size_t{100});
        req(outer.try_charge(50), "the outer scope admits");
        {
            gpu::DeviceBudget inner(std::size_t{10});
            req(!inner.try_charge(20), "the inner scope has its own limit");
            req(gpu::DeviceBudget::current() == &inner, "the inner scope is installed");
        }
        req(gpu::DeviceBudget::current() == &outer, "the outer scope is restored");
        req(outer.charged() == 50, "outer charges survive inner nesting");
        req(!outer.try_charge(60), "the outer meter never saw the inner refusal");
    }
    req(gpu::DeviceBudget::current() == nullptr, "no scope leaks past its lifetime");
}

// Over-limit charges refuse BEFORE the allocator runs (contract §2): the
// exception names the budget and the meter never sees the refused bytes.
void test_buffer_layer_refuses_before_allocator() {
    {
        std::size_t peak = 0;
        gpu::DeviceBudget budget(std::size_t{1024}, &peak);
        bool refused = false;
        try {
            gpu::DeviceBuffer<double> too_big(std::size_t{1025});  // 8200 B > 1024
        } catch (const gpu::DeviceBudgetExhausted&) {
            refused = true;
        }
        req(refused, "an over-limit device buffer throws DeviceBudgetExhausted");
        req(budget.charged() == 0, "the refused charge never reached the allocator");
        req(budget.exhausted(), "the buffer refusal marks the budget exhausted");
    }
    {
        std::size_t peak = 0;
        gpu::DeviceBudget budget(std::size_t{1024}, &peak);
        {
            gpu::DeviceBuffer<double> fits(std::size_t{64});  // 512 B <= 1024
            req(fits.size() == 64, "an admitted buffer allocates");
        }
        req(budget.charged() == 0, "released buffers uncharge the meter");
        req(budget.high_water() == 512, "the admitted buffer is the high-water mark");
        req(!budget.exhausted(), "an admitted charge keeps the budget open");
    }
}

// Live-outstanding churn (contract §4): repeated alloc/free stays inside the
// limit — the meter tracks what is outstanding, not what was ever allocated.
void test_churn_stays_inside_limit() {
#ifdef MARKOV_CERO_HAS_CUDA
    if (!gpu::is_gpu_available()) return;  // no device to churn against
#endif
    std::size_t peak = 0;
    gpu::DeviceBudget budget(std::size_t{1u << 20}, &peak);
    for (int iteration = 0; iteration < 100; ++iteration) {
        gpu::DeviceBuffer<double> a(std::size_t{4096});  // 32 KiB live
        gpu::DeviceBuffer<double> b(std::size_t{4096});
        req(a.data() != nullptr && b.data() != nullptr, "churn buffers allocate");
    }
    req(budget.charged() == 0, "every churn iteration released its charge");
    req(budget.high_water() <= (std::size_t{1u << 20}),
        "live-outstanding churn never crosses the limit");
    req(!budget.exhausted(), "a within-limit churn never exhausts the budget");
}

// Solve behavior (contract §5): a 1-byte device limit on a host that reaches
// the gpu buffer layer refuses with its own reason; a CUDA host without a
// usable device falls back to CPU and solves with an empty stop and a zero
// peak. The same solve with the limit unset accounts and never refuses.
void test_solve_behavior() {
    api::SolveOptions options;
    options.engine = "pdlp";
    options.backend = "gpu";
    options.device_memory_limit_bytes = 1;
#ifdef MARKOV_CERO_HAS_CUDA
    const bool device_layer_runs = gpu::is_gpu_available();
#else
    // CPU-only builds still run the gpu layer over its host-backed buffers.
    const bool device_layer_runs = true;
#endif
    const api::SolveResult limited = api::solve_file("examples/blend.mps", options);
    if (device_layer_runs) {
        req(limited.status == lp::reference::SolveStatus::resource_limit,
            "a 1-byte device limit stops the solve with resource_limit");
        req(limited.stop_reason == "device_memory_budget_exhausted",
            "the stop is attributed to the device budget");
        req(limited.diagnostic.failure_site == "device_memory_budget",
            "the device failure site is preserved");
        req(limited.diagnostic.suggested_recovery ==
                "raise_device_memory_limit_bytes_or_reduce_the_model",
            "the recovery names the device budget option");
        req(!limited.verified, "a device-limited result is never verified");
        req(limited.device_memory_charged_peak_bytes <= 1,
            "admitted device bytes never exceed the limit");
    } else {
        req(limited.status == lp::reference::SolveStatus::optimal ||
                limited.status == lp::reference::SolveStatus::feasible,
            "a CUDA host without a device still solves via CPU fallback");
        req(limited.stop_reason.empty(), "a fallback records no stop reason");
        req(limited.device_memory_charged_peak_bytes == 0,
            "no device budget charge ran on the fallback path");
    }
    options.device_memory_limit_bytes.reset();
    const api::SolveResult control = api::solve_file("examples/blend.mps", options);
    req(control.status == lp::reference::SolveStatus::optimal ||
            control.status == lp::reference::SolveStatus::feasible,
        "the same solve without a limit still succeeds");
    req(control.stop_reason.empty(), "an unlimited device budget never stops a solve");
    req((control.device_memory_charged_peak_bytes > 0) == device_layer_runs,
        "the peak reports exactly what the device layer admitted");
}

} // namespace

int main() {
    test_meter_semantics();
    test_buffer_layer_refuses_before_allocator();
    test_churn_stays_inside_limit();
    test_solve_behavior();
    std::cout << "device budget tests passed\n";
    return 0;
}
