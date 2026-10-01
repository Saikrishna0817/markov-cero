#pragma once
#include "markov_cero/nlp/sqp_solver.hpp"

#include "markov_cero/nlp/lbfgs.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "nlp_callback_guard.hpp"
#include "nlp_helpers.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace markov_cero::nlp {
namespace detail_sqp_solver {}
namespace detail_sqp_solver {
constexpr double kInf = std::numeric_limits<double>::infinity();
// NLP-01 section 4.2 / NLP-02 section 4.1: shared trust cap on the
// infinity norm of any QP step (standard step and elastic step alike).
constexpr double kTrustRadius = 0.5;
}
namespace detail_sqp_solver {
constexpr double kActiveTolerance = 1e-10;
}
namespace detail_sqp_solver { double stationarity_violation(const NlpModel& model, const std::vector<double>& x,
                              const std::vector<double>& stat); }
namespace detail_sqp_solver { qp::QuadraticModel build_subproblem(const NlpModel& model,
                                    const std::vector<double>& x,
                                    const std::vector<double>& grad,
                                    const Lbfgs& lbfgs,
                                    const std::vector<std::vector<double>>& J,
                                    const std::vector<double>& cvals,
                                    std::size_t n_ineq); }
namespace detail_sqp_solver { double dot_vectors(const std::vector<double>& a, const std::vector<double>& b); }
namespace detail_sqp_solver { double merit_slope_from(const std::vector<double>& grad,
                        const std::vector<double>& cvals,
                        const std::vector<std::vector<double>>& J,
                        std::size_t n_ineq, const std::vector<double>& d, double mu); }
namespace detail_sqp_solver { double merit_directional_derivative(const NlpModel& model, const std::vector<double>& z,
                                    const std::vector<double>& d, double mu); }
namespace detail_sqp_solver {
/// Shared QP options for the standard and the elastic SQP subproblem
/// (nlp-restoration.md section 3.4): identical tolerances, iteration cap
/// and the shared solve deadline.
inline qp::QpOptions sqp_qp_options(const SqpOptions& options) {
    qp::QpOptions qp_opts;
    qp_opts.absolute_tolerance = 1e-8;
    qp_opts.relative_tolerance = 1e-8;
    qp_opts.max_iterations = 20000;
    qp_opts.time_limit_seconds = 10.0;
    qp_opts.deadline = options.deadline;
    return qp_opts;
}
}
namespace detail_sqp_solver { qp::QuadraticModel build_elastic_subproblem(const NlpModel& model,
                                     const std::vector<double>& x,
                                     const std::vector<double>& grad,
                                     const Lbfgs& lbfgs,
                                     const std::vector<std::vector<double>>& J,
                                     const std::vector<double>& cvals,
                                     std::size_t n_ineq, double rho); }
namespace detail_sqp_solver {
/// NLP-02 contract nlp-restoration.md section 4: one bounded restoration
/// attempt — solve the slack-penalized QP, trust-cap the d step, backtrack
/// until the ORIGINAL constraint violation drops by the documented factor.
/// Rejects (escalating rho) when no trial passes A1/A2.
struct RestorationOutcome {
    bool accepted{false};
    std::vector<double> x_trial;
    double violation{0.0};
};
RestorationOutcome attempt_restoration(const NlpModel& model, const SqpOptions& options,
                                       const std::vector<double>& x,
                                       const std::vector<double>& grad, const Lbfgs& lbfgs,
                                       const std::vector<std::vector<double>>& J,
                                       const std::vector<double>& cvals,
                                       std::size_t n_ineq, double mu, double& rho);
}
namespace detail_sqp_solver { std::string restoration_exhausted_message(const SqpSolution& sol,
                                     const SqpOptions& options); }
namespace detail_sqp_solver {
/// NLP-01 contract §4.2-4.3: trust-cap the QP step, then the bracketed
/// Armijo-Wolfe search on the l1 merit (with the documented Wolfe waiver).
/// Returns the accepted trial point, or accepted=false when no step passed.
struct LineSearchOutcome {
    bool accepted{false};
    std::vector<double> x_trial;
};
LineSearchOutcome line_search(const NlpModel& model, const SqpOptions& options,
                              const std::vector<double>& x, std::vector<double> d,
                              const std::vector<double>& grad,
                              const std::vector<double>& cvals,
                              const std::vector<std::vector<double>>& J,
                              std::size_t n_ineq, double mu);
}
SqpSolution solve_sqp(const NlpModel& model, const std::vector<double>& x0,
                      const SqpOptions& options);
}
