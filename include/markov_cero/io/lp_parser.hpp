#pragma once

#include "markov_cero/model/model.hpp"

#include <string>

namespace markov_cero::io {

/// Sovereign CPLEX LP format parser.
/// Parses standard CPLEX .lp files supporting Minimize/Maximize, Subject To,
/// Bounds, Binary, General/Integer, and End sections.
[[nodiscard]] model::Model parse_lp_string(const std::string& content);
[[nodiscard]] model::Model parse_lp_file(const std::string& path);

} // namespace markov_cero::io
