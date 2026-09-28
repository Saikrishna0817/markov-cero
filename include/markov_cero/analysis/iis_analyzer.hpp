#pragma once

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace markov_cero::analysis {

struct IisConstraint {
    std::size_t row_index{0};
    std::string row_name;
    double lower_bound{0.0};
    double upper_bound{0.0};
    std::string description;
};

struct IisResult {
    bool is_infeasible{false};
    bool complete{false}; // Row-irreducible relative to the unchanged variable bounds.
    std::vector<IisConstraint> irreducible_subsystem;
    std::string diagnostic_summary;
    std::size_t lps_solved{0};
    double analysis_time_ms{0.0};
};

/// Computes an Irreducible Infeasible Subsystem (IIS) using Chinneck-Dravnieks deletion filtering.
/// Identifies the minimal set of mutually conflicting constraints that cause model infeasibility.
struct IisOptions {
    std::optional<std::chrono::steady_clock::time_point> deadline;
    double time_limit_seconds{30};
};
// Continuous linear rows only; integer infeasibility and bound-minimality are unsupported.
[[nodiscard]] IisResult compute_iis(const model::Model& model, const IisOptions& = {});

} // namespace markov_cero::analysis
