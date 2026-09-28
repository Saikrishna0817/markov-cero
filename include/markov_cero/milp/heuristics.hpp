#pragma once

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace markov_cero::milp {

struct HeuristicResult {
    bool found{false};
    std::vector<double> primal;
    double objective{0.0};
};

[[nodiscard]] bool check_integer_feasibility(const model::Model& model,
                                             const std::vector<double>& primal,
                                             double feasibility_tol = 1e-6,
                                             double integrality_tol = 1e-6);
[[nodiscard]] bool check_integer_feasibility(const model::Model& model,
    const std::vector<model::Bound>& variable_lower,
    const std::vector<model::Bound>& variable_upper, const std::vector<double>& primal,
    double feasibility_tol, double integrality_tol);

[[nodiscard]] double compute_objective(const model::Model& model,
                                       const std::vector<double>& primal);

[[nodiscard]] HeuristicResult simple_rounding(const model::Model& model,
                                              const std::vector<double>& continuous_primal,
                                              double feasibility_tol = 1e-6,
                                              double integrality_tol = 1e-6);
[[nodiscard]] HeuristicResult simple_rounding(const model::Model& model,
    const std::vector<model::Bound>& variable_lower,
    const std::vector<model::Bound>& variable_upper,
    const std::vector<double>& continuous_primal, double feasibility_tol,
    double integrality_tol);

[[nodiscard]] HeuristicResult feasibility_pump(const model::Model& model,
                                               const std::vector<double>& continuous_primal,
                                               std::size_t max_iterations = 10,
                                               double feasibility_tol = 1e-6,
                                               double integrality_tol = 1e-6);

[[nodiscard]] HeuristicResult coefficient_diving(const model::Model& model,
                                                 const std::vector<double>& continuous_primal,
                                                 std::size_t max_depth = 50,
                                                 double feasibility_tol = 1e-6,
                                                 double integrality_tol = 1e-6);

[[nodiscard]] HeuristicResult local_swap_repair(const model::Model& model,
                                               const std::vector<double>& candidate_primal,
                                               std::size_t max_swaps = 20,
                                               double feasibility_tol = 1e-6,
                                               double integrality_tol = 1e-6);

} // namespace markov_cero::milp
