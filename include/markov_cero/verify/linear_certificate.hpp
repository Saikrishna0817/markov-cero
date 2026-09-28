#pragma once
#include "markov_cero/model/model.hpp"
#include <limits>
#include <string>
#include <vector>
namespace markov_cero::verify {
struct LinearCertificate {
    bool accepted{false};
    double bound{std::numeric_limits<double>::quiet_NaN()};
    double relative_gap{std::numeric_limits<double>::infinity()};
    std::string message;
};
// Row multipliers follow c + A^T y, with positive y selecting the row upper bound.
[[nodiscard]] LinearCertificate verify_linear_solution(
    const model::Model&, const std::vector<double>& primal,
    const std::vector<double>& row_dual, double objective, double tolerance = 1e-7,
    bool relax_integrality = false);
} // namespace markov_cero::verify
