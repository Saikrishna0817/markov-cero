#pragma once
#include "markov_cero/qp/model.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace markov_cero::qp {
namespace detail_model {}

ConvexityReport assess_convexity(const SparseSymmetricMatrix& P, double tolerance,
                                std::size_t maximum_factor_nonzeros,
                                std::optional<std::chrono::steady_clock::time_point> deadline);
bool check_convexity(const SparseSymmetricMatrix& P, double tolerance);
QuadraticModel make_quadratic_model(const model::Model& model);
}
