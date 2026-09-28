#pragma once
#include "markov_cero/transform/canonicalize.hpp"
#include <cstddef>
#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <vector>
namespace markov_cero::lp::reference {
enum class SolveStatus {
    optimal,
    infeasible,
    unbounded,
    iteration_limit,
    invalid_model,
    invalid_options,
    resource_limit,
    numerical_failure,
    unsupported,
    non_convex_minlp
};
struct Options {
    std::size_t iteration_limit{10000};
    std::size_t telemetry_limit{10000};
    double feasibility_tolerance{1e-9};
    double dual_tolerance{1e-9};
    double pivot_tolerance{1e-12};
    bool bland_anti_cycling{true};
    double time_limit_seconds{std::numeric_limits<double>::infinity()};
    // Absolute wall-clock deadline shared by the API's sequential fallbacks.
    // Empty means no deadline (library callers retain the historical behavior).
    std::optional<std::chrono::steady_clock::time_point> deadline;
};
struct IterationRecord {
    std::size_t iteration{};
    int phase{};
    double objective{};
    double minimum_reduced_cost{};
    std::size_t entering{};
    std::size_t leaving{};
    bool degenerate{};
};
struct Result {
    SolveStatus status{SolveStatus::numerical_failure};
    std::vector<double> primal;
    std::vector<double> dual;
    std::vector<double> ray;
    std::vector<double> certificate;
    std::vector<std::size_t> basis;
    double objective{};
    std::size_t phase_one_iterations{};
    std::size_t phase_two_iterations{};
    std::size_t bound_flips{};
    bool telemetry_truncated{};
    // Pivot-ratio condition proxy (max|Uii| / min|Uii|) of the final basis
    // factorization. 0.0 means "no factorization was performed" (e.g. the
    // model was rejected before any basis existed).
    double condition_estimate{0.0};
    std::vector<IterationRecord> telemetry;
    std::string message;
};
[[nodiscard]] Result solve(const transform::CanonicalModel& model, const Options& options = {});
[[nodiscard]] const char* to_string(SolveStatus status) noexcept;
} // namespace markov_cero::lp::reference
