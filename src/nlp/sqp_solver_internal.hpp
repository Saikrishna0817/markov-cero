#pragma once
#include "markov_cero/nlp/sqp_solver.hpp"

#include "markov_cero/nlp/lbfgs.hpp"
#include "markov_cero/qp/admm_solver.hpp"
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
SqpSolution solve_sqp(const NlpModel& model, const std::vector<double>& x0,
                      const SqpOptions& options);
}
