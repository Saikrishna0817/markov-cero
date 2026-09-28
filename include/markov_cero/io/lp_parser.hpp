#pragma once

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace markov_cero::io {

struct LpLimits final {
    std::size_t maximum_bytes{16U * 1024U * 1024U};
    std::size_t maximum_tokens{1'000'000U};
    std::size_t maximum_rows{1'000'000U};
    std::size_t maximum_columns{1'000'000U};
    std::size_t maximum_nonzeros{20'000'000U};
    std::size_t maximum_quadratic_terms{1'000'000U};
    std::size_t maximum_name_bytes{255U};
};

class LpResourceLimitError final : public std::length_error {
  public:
    using std::length_error::length_error;
};

/// Sovereign CPLEX LP format parser.
/// Parses standard CPLEX .lp files supporting Minimize/Maximize, Subject To,
/// Bounds, Binary, General/Integer, and End sections.
[[nodiscard]] model::Model parse_lp_string(const std::string& content,
                                           const LpLimits& limits = {});
[[nodiscard]] model::Model parse_lp_file(const std::string& path,
                                         const LpLimits& limits = {});

} // namespace markov_cero::io
