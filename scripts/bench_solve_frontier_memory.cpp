#include "frontier_storage_bytes.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/milp/branch_node.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/model/model.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <sys/time.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <thread>

// Full-solver frontier/RSS evidence for W01/IR-19: run a real MILP solve and
// report the structural storage classes next to the measured peak RSS, so
// frontier growth (max_queued_nodes) can be related to what the queue records
// alone could cost. queue_records_bytes is a lower bound: it counts heap
// records only (delta chains, shared cut payloads and LP workspaces excluded).

namespace {

// argv[argc] is the only guaranteed out-of-range slot: scan for it before
// touching argv[index], otherwise an omitted trailing flag reads past the
// terminator (undefined behaviour).
int argc_of(char** argv) {
    int count = 0;
    while (argv[count]) ++count;
    return count;
}

std::size_t argument(char** argv, int index, std::size_t fallback) {
    if (index >= argc_of(argv)) return fallback;
    if (argv[index][0] == '-')
        throw std::invalid_argument("benchmark arguments cannot be negative");
    char* end = nullptr;
    const auto parsed = std::strtoull(argv[index], &end, 10);
    if (end == argv[index] || *end != '\0' || parsed == 0)
        throw std::invalid_argument("benchmark arguments must be positive integers");
    return static_cast<std::size_t>(parsed);
}

std::size_t flag(char** argv, int index, std::size_t fallback) {
    if (index >= argc_of(argv)) return fallback;
    if (std::string(argv[index]) != "0" && std::string(argv[index]) != "1")
        throw std::invalid_argument("flag arguments must be 0 or 1");
    return static_cast<std::size_t>(argv[index][0] - '0');
}

std::size_t peak_rss_kib() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        throw std::runtime_error("getrusage failed");
    return static_cast<std::size_t>(usage.ru_maxrss);
}

std::size_t resident_kib() {
    std::ifstream statm("/proc/self/statm");
    std::size_t total_pages = 0;
    std::size_t resident_pages = 0;
    if (statm >> total_pages >> resident_pages) return resident_pages * 4;
    return 0;
}

// Samples resident memory during the solve so an intermittent RSS spike can
// be placed in time (root phase / node search / deadline edge) instead of
// only being visible as a final high-water mark.
class ResidentSampler final {
  public:
    ResidentSampler()
        : started_(std::chrono::steady_clock::now()),
          thread_([this] {
              while (!stop_.load(std::memory_order_relaxed)) {
                  const std::size_t kib = resident_kib();
                  if (kib > peak_kib_.load(std::memory_order_relaxed)) {
                      peak_kib_.store(kib, std::memory_order_relaxed);
                      const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - started_)
                                           .count();
                      peak_ms_.store(static_cast<std::size_t>(ms), std::memory_order_relaxed);
                  }
                  std::this_thread::sleep_for(std::chrono::milliseconds(10));
              }
          }) {}
    ~ResidentSampler() {
        stop_.store(true, std::memory_order_relaxed);
        thread_.join();
    }
    [[nodiscard]] std::size_t peak_kib() const { return peak_kib_.load(); }
    [[nodiscard]] std::size_t peak_ms() const { return peak_ms_.load(); }

  private:
    std::chrono::steady_clock::time_point started_;
    std::atomic<bool> stop_{false};
    std::atomic<std::size_t> peak_kib_{0};
    std::atomic<std::size_t> peak_ms_{0};
    std::thread thread_;
};


// Tick capture (single-threaded, unlike ResidentSampler): records ru_maxrss
// on a SIGALRM cadence so a transient spike can be localized in time without
// creating a second thread (which would change malloc arena behaviour).
struct AlarmCapture final {
    static volatile std::sig_atomic_t ticks;
    static volatile std::size_t peak_kib;
    static volatile std::size_t peak_tick;
    static void handler(int) {
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) == 0 &&
            static_cast<std::size_t>(usage.ru_maxrss) > peak_kib) {
            peak_kib = static_cast<std::size_t>(usage.ru_maxrss);
            peak_tick = ticks;
        }
        ticks = static_cast<std::sig_atomic_t>(ticks + 1);
    }
    AlarmCapture() {
        struct sigaction action {};
        action.sa_handler = &AlarmCapture::handler;
        sigaction(SIGALRM, &action, nullptr);
        struct itimerval timer {};
        timer.it_value.tv_usec = 250000;
        timer.it_interval.tv_usec = 250000;
        setitimer(ITIMER_REAL, &timer, nullptr);
    }
    ~AlarmCapture() {
        struct itimerval timer {};
        setitimer(ITIMER_REAL, &timer, nullptr);
    }
};
volatile std::sig_atomic_t AlarmCapture::ticks{0};
volatile std::size_t AlarmCapture::peak_kib{0};
volatile std::size_t AlarmCapture::peak_tick{0};

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "usage: solve_frontier_memory_benchmark INSTANCE.mps "
                         "[max_nodes=2000] [time_limit_seconds=30]\n";
            return 2;
        }
        const std::string path = argv[1];
        const std::size_t max_nodes = argument(argv, 2, 2000);
        const std::size_t time_limit_seconds = argument(argv, 3, 30);
        const std::size_t strong_branching = flag(argv, 4, 1);
        const std::size_t cuts = flag(argv, 5, 1);

        std::ifstream input(path);
        if (!input) throw std::invalid_argument("cannot open instance: " + path);
        const auto model = markov_cero::io::parse_mps(input);

        markov_cero::milp::Options options;
        options.max_nodes = max_nodes;
        options.time_limit_seconds = static_cast<double>(time_limit_seconds);
        const std::size_t rss_after_parse = peak_rss_kib();
        options.enable_strong_branching = strong_branching != 0;
        options.enable_cuts = cuts != 0;
        options.enable_mir_cuts = cuts != 0;
        ResidentSampler sampler;
        AlarmCapture alarm;
        const auto result = markov_cero::milp::solve(model, options);

        std::cout << "{\"mode\":\"solve-frontier\",\"instance\":\"" << path << "\""
                  << ",\"variables\":" << model.matrix.column_count
                  << ",\"rows\":" << model.matrix.row_count
                  << ",\"root_matrix_nnz\":" << frontier_storage_bytes::root_nnz(model)
                  << ",\"max_nodes_cap\":" << max_nodes
                  << ",\"time_limit_seconds\":" << time_limit_seconds
                  << ",\"status\":\"" << markov_cero::lp::reference::to_string(result.status)
                  << "\""
                  << ",\"nodes\":" << result.nodes_explored
                  << ",\"max_queued_nodes\":" << result.max_queued_nodes
                  << ",\"lp_iterations\":" << result.lp_iterations
                  << ",\"root_bytes\":" << frontier_storage_bytes::root(model)
                  << ",\"strong_branching\":" << strong_branching
                  << ",\"cuts\":" << cuts
                  << ",\"rss_after_parse_kib\":" << rss_after_parse
                  << ",\"queue_records_bytes\":"
                  << result.max_queued_nodes * sizeof(markov_cero::milp::BranchNode)
                  << ",\"peak_rss_kib\":" << peak_rss_kib()
                  << ",\"sampler_peak_kib\":" << sampler.peak_kib()
                  << ",\"sampler_peak_ms\":" << sampler.peak_ms()
                  << ",\"alarm_peak_kib\":" << AlarmCapture::peak_kib
                  << ",\"alarm_peak_tick\":" << AlarmCapture::peak_tick
                  << ",\"alarm_ticks\":" << AlarmCapture::ticks
                  << ",\"runtime_ms\":" << static_cast<std::size_t>(result.runtime_ms) << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
