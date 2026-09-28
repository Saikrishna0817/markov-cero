#pragma once
#include "convergence.hpp"
#include "markov_cero/qp/admm_solver.hpp"

#include "markov_cero/gpu/admm_step.hpp"
#include "markov_cero/gpu/device.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace markov_cero::qp {
namespace detail_admm_solver {}
namespace detail_admm_solver {
constexpr std::size_t kGpuQpNnzThreshold = 100000;
}
const char* to_string(QpStatus status) noexcept;
namespace detail_admm_solver { double inf_norm(const std::vector<double>& v) noexcept; }
namespace detail_admm_solver { std::vector<double> multiply_A(const linalg::SparseCsc& A,
                               const std::vector<double>& x); }
namespace detail_admm_solver { std::vector<double> multiply_AT(const linalg::SparseCsc& A,
                                const std::vector<double>& y); }
namespace detail_admm_solver { double project_bound(double v, double l, double u) noexcept; }
namespace detail_admm_solver { bool infeasibility_certificate(const QuadraticModel& model, const QpOptions& options,
    const std::vector<double>& x, const std::vector<double>& y,
    const std::vector<double>& x_prev, const std::vector<double>& y_prev, QpSolution& sol); }
}
