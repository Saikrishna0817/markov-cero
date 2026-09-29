// W02 / backlog item 3: worker isolation boundary.
//
// Defines what is shared across search workers (deadline, cancellation, one
// memory budget, the solve-wide stop reason) and what stays private to a
// worker (charge attribution, release rights, a local stop reason), and
// proves the boundary under concurrent charge/release.

#include "markov_cero/core/solve_context.hpp"
#include "markov_cero/core/worker_context.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using markov_cero::core::SolveContext;
using markov_cero::core::StopReason;
using markov_cero::core::TraceEvent;
using markov_cero::core::TraceSink;
using markov_cero::core::WorkerContext;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct RecordingSink final : TraceSink {
    std::vector<TraceEvent> events;
    void emit(const TraceEvent& event) noexcept override { events.push_back(event); }
};

void test_local_stop_is_isolated() {
    SolveContext shared;
    {
        WorkerContext first(shared, 0);
        WorkerContext second(shared, 1);

        require(first.note_local_stop(StopReason::quota_exhausted) == StopReason::quota_exhausted,
                "a worker records its own stop reason");
        require(first.local_stop_reason() == StopReason::quota_exhausted,
                "the local stop is sticky for its worker");
        require(second.local_stop_reason() == StopReason::none,
                "a sibling worker does not inherit the local stop");
        require(shared.stop_reason() == StopReason::none,
                "a local stop does not pollute the solve-wide stop reason");
        require(first.poll() == StopReason::quota_exhausted,
                "the stopped worker reports its own stop");
        require(second.poll() == StopReason::none,
                "an unstopped sibling keeps polling the shared context");
        require(first.note_local_stop(StopReason::cancelled) == StopReason::quota_exhausted,
                "the first local reason wins");
    }
    require(shared.stop_reason() == StopReason::none,
            "worker scope exit does not invent a solve-wide stop");
}

void test_attribution_and_release_rights() {
    SolveContext::Config config;
    config.memory_limit_bytes = 1000;
    SolveContext budgeted(config);
    {
        WorkerContext first(budgeted, 0);
        WorkerContext second(budgeted, 1);

        require(first.charge(400), "a charge inside the shared budget is admitted");
        require(second.charge(400), "a second worker shares the same budget");
        require(first.charged_by_worker() == 400 && second.charged_by_worker() == 400,
                "each worker tracks only its own bytes");
        require(budgeted.memory().charged() == 800, "the shared budget sees both charges");
        require(budgeted.memory().high_water() == 800, "the shared high-water mark is recorded");

        // A mis-balanced release from one worker cannot refund more than that
        // worker charged, so it cannot under-count another worker's bytes.
        second.release(1000);
        require(second.charged_by_worker() == 0, "the worker drops its whole charge");
        require(budgeted.memory().charged() == 400,
                "a release never refunds bytes the worker did not hold");
        first.release(100);
        require(first.charged_by_worker() == 300, "partial releases are tracked");
        require(budgeted.memory().charged() == 300, "the shared counter follows");
    }
    require(budgeted.memory().charged() == 0, "worker scope exit returns every held byte");
    require(budgeted.memory().high_water() == 800, "the high-water mark survives cleanup");
    require(!budgeted.memory().exhausted(),
            "a budget that was never overdrawn never becomes exhausted");
}

void test_shared_exhaustion_stops_every_worker() {
    SolveContext::Config config;
    config.memory_limit_bytes = 100;
    SolveContext shared(config);
    {
        WorkerContext first(shared, 0);
        WorkerContext second(shared, 1);

        require(first.charge(60), "the first worker is admitted");
        require(!second.charge(60), "the second worker is refused at the shared limit");
        require(shared.stop_reason() == StopReason::memory_budget_exhausted,
                "budget exhaustion is a solve-wide stop");
        require(second.local_stop_reason() == StopReason::memory_budget_exhausted,
                "the refused worker stops immediately");
        require(!first.charge(10), "the shared budget is sticky for every worker");
        require(first.local_stop_reason() == StopReason::memory_budget_exhausted,
                "a later refusal also stops the worker that asked");
        require(first.poll() == StopReason::memory_budget_exhausted &&
                    second.poll() == StopReason::memory_budget_exhausted,
                "every worker polls the solve-wide stop first");
        require(second.charged_by_worker() == 0, "a refused charge attributes nothing");
    }
    require(shared.memory().charged() == 0, "worker scope exit returns every held byte");
    require(shared.memory().exhausted(), "exhaustion stays sticky after release");
    require(shared.stop_reason() == StopReason::memory_budget_exhausted,
            "the solve-wide stop reason is unchanged by cleanup");
}

void test_attribution_overflow_fails_closed() {
    SolveContext shared;
    {
        WorkerContext worker(shared, 0);
        require(worker.charge(std::numeric_limits<std::size_t>::max()),
                "an unlimited budget admits a maximal charge");
        require(!worker.charge(1), "attribution overflow is refused before it can wrap");
        require(shared.stop_reason() == StopReason::memory_budget_exhausted,
                "the refusal is reported as a solve-wide memory stop");
        require(shared.memory().exhausted(), "the shared budget fails closed");
    }
    require(shared.memory().charged() == 0, "the maximal charge is returned on destruction");
}

void test_worker_events_are_tagged() {
    SolveContext shared;
    RecordingSink sink;
    shared.set_trace_sink(&sink);
    WorkerContext worker(shared, 3);
    worker.emit(TraceEvent{"node_lp", StopReason::none, 5, 128, 1.25, 0});
    require(sink.events.size() == 1, "the shared sink receives worker events");
    require(sink.events[0].worker == 3, "the event carries the worker index");
    require(sink.events[0].stage == std::string("node_lp") && sink.events[0].count == 5,
            "the event payload is preserved");
    shared.set_trace_sink(nullptr);
}

void test_concurrent_workers_share_one_budget() {
    const std::size_t limit = 1000;
    const std::size_t chunk = 128;
    const std::size_t attempts = 40;
    SolveContext::Config config;
    config.memory_limit_bytes = limit;
    SolveContext shared(config);

    std::atomic<std::size_t> refusals{0};
    std::atomic<std::size_t> admitted{0};
    std::atomic<bool> accounting_ok{true};
    std::vector<std::thread> workers;
    for (std::size_t index = 0; index < 4; ++index) {
        workers.emplace_back([&, index] {
            WorkerContext worker(shared, index);
            std::size_t held = 0;
            for (std::size_t attempt = 0; attempt < attempts; ++attempt) {
                if (worker.charge(chunk)) {
                    held += chunk;
                    admitted.fetch_add(1, std::memory_order_relaxed);
                } else {
                    refusals.fetch_add(1, std::memory_order_relaxed);
                }
            }
            worker.release(held / 2);
            // A worker's own accounting must stay exact under contention.
            if (worker.charged_by_worker() != held - held / 2) accounting_ok.store(false);
        });
    }
    for (auto& worker : workers) worker.join();

    require(accounting_ok.load(), "each worker's own accounting stays exact under contention");
    require(refusals.load() > 0, "the shared limit is actually exercised");
    require(shared.memory().high_water() <= limit, "the shared budget is never overshot");
    require(shared.memory().charged() == 0,
            "every worker returns its held bytes through its destructor");
    require(shared.stop_reason() == StopReason::memory_budget_exhausted,
            "exhaustion under contention is reported once, solve-wide");
    require(admitted.load() * chunk >= shared.memory().high_water(),
            "the high-water mark is consistent with admitted charges");
}
} // namespace

int main() {
    test_local_stop_is_isolated();
    test_attribution_and_release_rights();
    test_shared_exhaustion_stops_every_worker();
    test_attribution_overflow_fails_closed();
    test_worker_events_are_tagged();
    test_concurrent_workers_share_one_budget();
    return 0;
}
