#include "markov_cero/core/instrumentation.hpp"
#include "markov_cero/core/solve_context.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using markov_cero::core::CapabilityPolicy;
using markov_cero::core::Deadline;
using markov_cero::core::MemoryBudget;
using markov_cero::core::StageScope;
using markov_cero::core::StopReason;
using markov_cero::core::SolveContext;
using markov_cero::core::TraceEvent;
using markov_cero::core::TraceSink;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct RecordingSink final : TraceSink {
    std::vector<TraceEvent> events;
    void emit(const TraceEvent& event) noexcept override { events.push_back(event); }
};

void test_deadline_semantics() {
    Deadline none;
    require(!none.has_deadline(), "default deadline is empty");
    require(!none.expired(), "empty deadline never expires");
    require(!none.remaining(), "empty deadline has no remaining time");

    const Deadline soon = Deadline::in(std::chrono::milliseconds(5));
    require(soon.has_deadline(), "in() sets a deadline");
    require(!soon.expired(), "fresh deadline is not expired");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    require(soon.expired(), "deadline expires after its instant passes");
    const auto left = soon.remaining();
    require(left && *left == Deadline::Clock::duration::zero(),
            "expired deadline reports zero remaining");

    const Deadline later = Deadline::in(std::chrono::seconds(60));
    const auto original = later.time_point();
    Deadline intersection = later;
    intersection.shorten_to(Deadline::in(std::chrono::seconds(1)));
    require(intersection.time_point() && *intersection.time_point() < *original,
            "shorten_to adopts the earlier instant");
    Deadline extended = Deadline::in(std::chrono::seconds(1));
    const auto short_instant = extended.time_point();
    extended.shorten_to(later);
    require(extended.time_point() && *extended.time_point() == *short_instant,
            "shorten_to never extends a deadline");
    Deadline empty;
    empty.shorten_to(later);
    require(empty.has_deadline(), "shorten_to fills an empty deadline");
    empty.clear();
    require(!empty.has_deadline(), "clear resets the deadline");
}

void test_memory_budget_preflight() {
    MemoryBudget budget(1000);
    require(budget.limit() && *budget.limit() == 1000, "limit is reported");
    require(budget.try_charge(400), "charge inside the limit is admitted");
    require(budget.charged() == 400, "charged bytes are tracked");
    require(budget.high_water() == 400, "high-water mark follows charges");
    require(budget.try_charge(600), "a charge that lands exactly on the limit is admitted");
    require(budget.available() && *budget.available() == 0, "no bytes remain at the limit");
    require(!budget.try_charge(1), "a charge past the limit is refused");
    require(budget.exhausted(), "refusal marks the budget exhausted");
    budget.release(1000);
    require(budget.charged() == 0, "release returns the charge to zero");
    require(budget.high_water() == 1000, "high-water mark is not undone by release");
    require(!budget.try_charge(1), "exhaustion is sticky after release");

    MemoryBudget floor_check;
    floor_check.release(50);
    require(floor_check.charged() == 0, "release floors at zero");
    require(!floor_check.exhausted(), "a surplus release does not exhaust");
}

void test_memory_budget_overflow_and_unlimited() {
    MemoryBudget unlimited;
    require(unlimited.unlimited(), "default budget is unlimited");
    require(!unlimited.limit(), "unlimited budget reports no limit");
    require(unlimited.try_charge(SIZE_MAX), "unlimited budget admits a maximal charge");
    require(!unlimited.try_charge(1), "an overflowing charge is refused instead of wrapping");
    require(unlimited.exhausted(), "refused overflow marks exhaustion");
    require(unlimited.charged() == SIZE_MAX, "charge counter did not wrap");

    MemoryBudget limited(SIZE_MAX);
    require(limited.try_charge(SIZE_MAX / 2U), "large charge inside a large limit is admitted");
    require(limited.try_charge(SIZE_MAX / 2U), "second half of the range is admitted");
    require(limited.try_charge(1), "a charge landing exactly on the limit is admitted");
    require(!limited.try_charge(1), "any further charge is refused");
    require(limited.exhausted(), "refused overflow is recorded");
    require(limited.charged() == SIZE_MAX, "counter reached the limit without wrapping");
}

void test_memory_budget_concurrency() {
    MemoryBudget budget(40000);
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&budget, &failed] {
            for (int index = 0; index < 10000; ++index) {
                if (!budget.try_charge(1)) {
                    failed.store(true);
                    return;
                }
                budget.release(1);
            }
        });
    }
    for (auto& worker : workers) worker.join();
    require(!failed.load(), "concurrent charges were never unexpectedly refused");
    require(budget.charged() == 0, "balanced concurrent charges net to zero");
    require(!budget.exhausted(), "charge/release under a sufficient limit never exhausts");
}

void test_stop_reason_mapping() {
    require(std::string(markov_cero::core::to_string(StopReason::none)) == "none",
            "none is named");
    require(std::string(markov_cero::core::to_string(
                StopReason::memory_budget_exhausted)) == "memory_budget_exhausted",
            "memory stop is named");
    require(!markov_cero::core::is_resource_stop(StopReason::none),
            "none is not a resource stop");
    require(markov_cero::core::is_resource_stop(StopReason::queue_capacity_exhausted),
            "queue exhaustion is a resource stop");
}

void test_sticky_deadline_stop() {
    SolveContext::Config config;
    config.deadline = Deadline::in(std::chrono::milliseconds(5));
    SolveContext context(config);
    require(!context.stopped(), "a fresh context is running");
    require(context.poll() == StopReason::none, "an unexpired deadline is not yet a stop");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    require(context.poll() == StopReason::deadline_exceeded, "poll records the deadline");
    context.request_cancel();
    require(context.poll() == StopReason::deadline_exceeded,
            "the first stop reason stays sticky when cancellation arrives later");
    require(context.stop_reason() == StopReason::deadline_exceeded, "sticky reason is readable");
    require(context.stopped(), "stopped() follows the recorded reason");
}

void test_cancel_and_memory_stop() {
    SolveContext::Config config;
    config.memory_limit_bytes = 64;
    SolveContext context(config);
    require(context.charge_or_stop(64), "a charge inside the budget is admitted");
    require(!context.charge_or_stop(1), "an over-budget charge is refused");
    require(context.stop_reason() == StopReason::memory_budget_exhausted,
            "budget refusal records the memory stop reason");
    context.request_cancel();
    require(context.stop_reason() == StopReason::memory_budget_exhausted,
            "memory stop precedes a later cancellation");

    SolveContext cancelled;
    cancelled.request_cancel();
    require(cancelled.cancel_requested(), "cancellation is observable");
    require(cancelled.poll() == StopReason::cancelled, "poll records cancellation");
}

void test_quotas_policy_and_trace() {
    SolveContext::Config config;
    config.thread_quota = 8;
    config.device_quota = 0;
    config.seed = 42;
    config.deterministic = false;
    CapabilityPolicy capabilities;
    capabilities.allow_gpu = true;
    capabilities.log_model_data = true;
    config.capabilities = capabilities;
    SolveContext context(config);
    require(context.thread_quota() == 8, "thread quota is carried");
    require(context.device_quota() == 0, "device quota is carried");
    require(context.seed() == 42, "seed is carried");
    require(!context.deterministic(), "deterministic mode is carried");
    require(context.capabilities().allow_gpu, "capability policy is carried");
    require(context.capabilities().allow_ml == false, "unset capabilities stay off");

    RecordingSink sink;
    context.set_trace_sink(&sink);
    context.emit(TraceEvent{"node_lp", StopReason::none, 3, 64, 1.5});
    require(sink.events.size() == 1, "the sink receives events");
    require(sink.events[0].count == 3 && sink.events[0].bytes == 64, "event payload survives");
    context.set_trace_sink(nullptr);
    context.emit(TraceEvent{"node_lp", StopReason::none, 1, 8, 0.5});
    require(sink.events.size() == 1, "removing the sink disables emission");

    SolveContext quiet;
    quiet.emit(TraceEvent{"parse", StopReason::none, 0, 0, 0.0});
    require(quiet.trace_sink() == nullptr, "no sink is installed by default");
}

void test_deadline_remaining_instrumentation() {
    SolveContext untimed;
    require(!untimed.remaining_ms(), "a solve without a deadline reports no remaining time");

    SolveContext::Config config;
    config.deadline = Deadline::in(std::chrono::milliseconds(50));
    SolveContext timed(config);
    const auto left = timed.remaining_ms();
    require(left && *left > 0.0 && *left <= 100.0, "remaining_ms reports the live slack");
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    const auto expired = timed.remaining_ms();
    require(expired && *expired == 0.0, "an expired deadline reports zero remaining");
    require(timed.poll() == StopReason::deadline_exceeded, "polling records the deadline stop");
}

void test_stage_scope_emits_one_event() {
    SolveContext::Config config;
    config.memory_limit_bytes = 1024;
    SolveContext context(config);
    RecordingSink sink;
    context.set_trace_sink(&sink);

    {
        StageScope stage(context, "presolve");
        stage.add_count(4);
        stage.set_count(9);
        require(context.charge_or_stop(128), "the stage can charge the shared budget");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    require(sink.events.size() == 1, "the stage scope emits exactly one event");
    const TraceEvent& event = sink.events.back();
    require(event.stage == std::string("presolve"), "the event names the stage");
    require(event.count == 9, "the stage's own counter is reported");
    require(event.bytes == 128, "the event reports bytes charged during the stage");
    require(event.elapsed_ms >= 1.0, "the event carries the observed duration");
    require(event.worker == 0, "solve-level stages report worker zero");
    require(event.reason == StopReason::none, "an unstopped stage reports no reason");

    {
        StageScope stage(context, "node_lp", 2);
        require(!context.charge_or_stop(1U << 20U), "the exhausted budget refuses");
    }
    require(sink.events.size() == 2, "the second stage emits as well");
    require(sink.events[1].reason == StopReason::memory_budget_exhausted,
            "a stopped stage reports the shared stop reason");
    require(sink.events[1].worker == 2, "the worker index is carried");
    require(sink.events[1].bytes == 0, "a refused charge contributes no bytes");

    context.set_trace_sink(nullptr);
    { StageScope stage(context, "verify"); }
    require(sink.events.size() == 2, "detaching the sink silences emission");
}
} // namespace

int main() {
    test_deadline_semantics();
    test_memory_budget_preflight();
    test_memory_budget_overflow_and_unlimited();
    test_memory_budget_concurrency();
    test_stop_reason_mapping();
    test_sticky_deadline_stop();
    test_cancel_and_memory_stop();
    test_quotas_policy_and_trace();
    test_deadline_remaining_instrumentation();
    test_stage_scope_emits_one_event();
    return 0;
}
