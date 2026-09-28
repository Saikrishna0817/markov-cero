#pragma once

#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace markov_cero::qp {

struct QpVerificationReport {
    bool passed{false};
    double maximum_primal_violation{0.0};
    double maximum_dual_violation{0.0};
    double maximum_complementarity_violation{0.0};
    double maximum_integrality_violation{0.0};
    double objective_discrepancy{0.0};
    std::string failure_reason;
};

/// Independently validates a candidate QP solution against KKT conditions.
[[nodiscard]] QpVerificationReport verify_qp_solution(
    const QuadraticModel& model,
    const QpSolution& solution,
    double tolerance = 1e-4);

[[nodiscard]] bool verify_qp_infeasibility(const QuadraticModel&, const QpSolution&, double tolerance = 1e-6);
[[nodiscard]] bool verify_qp_unbounded(const QuadraticModel&, const QpSolution&, double tolerance = 1e-6);

// Requires a PSD objective and dimension-checked finite witness.
[[nodiscard]] double supporting_lower_bound(const model::Model&, const QuadraticModel&, const QpSolution&);

} // namespace markov_cero::qp
