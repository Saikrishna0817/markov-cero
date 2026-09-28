#pragma once
#include <cmath>
namespace markov_cero::milp::detail {
template<class Options>
bool valid_search_options(const Options& o) {
    return std::isfinite(o.time_limit_seconds) && o.time_limit_seconds > 0 && o.time_limit_seconds <= 1e8 &&
        std::isfinite(o.relative_gap_tolerance) && o.relative_gap_tolerance >= 0 &&
        std::isfinite(o.absolute_gap_tolerance) && o.absolute_gap_tolerance >= 0 &&
        std::isfinite(o.integrality_tolerance) && o.integrality_tolerance > 0 && o.integrality_tolerance < .5 &&
        std::isfinite(o.feasibility_tolerance) && o.feasibility_tolerance > 0;
}
}
