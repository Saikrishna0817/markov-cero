#pragma once
#include "markov_cero/minlp/minlp_solver.hpp"

#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/qp/model.hpp"

#include "../nlp/nlp_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace markov_cero::minlp {
namespace detail_minlp_solver {}
namespace detail_minlp_solver {
constexpr double kInf = std::numeric_limits<double>::infinity();
}
namespace detail_minlp_solver {
using SymmetricEntries = std::map<std::pair<std::size_t, std::size_t>, double>;
}
namespace detail_minlp_solver {
class UnsupportedMinlp final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
}
namespace detail_minlp_solver {
class NonConvexMinlp final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
}
namespace detail_minlp_solver { qp::SparseSymmetricMatrix make_symmetric_matrix(std::size_t n,
                                                 const SymmetricEntries& entries); }
namespace detail_minlp_solver { qp::ConvexityReport hessian_convexity(std::size_t n,
                                     const std::vector<model::NlobjTerm>& terms,
                                     const SymmetricEntries& base, double sign); }
namespace detail_minlp_solver { void require_convex_quadratic_structure(const model::Model& source); }
namespace detail_minlp_solver { model::Model build_master(const NlpModel& nlp,
                          const std::vector<std::size_t>& integer_indices,
                          const std::vector<std::vector<double>>& obj_grads,
                          const std::vector<double>& obj_rhs,
                          const std::vector<std::vector<double>>& cut_grads,
                          const std::vector<double>& cut_rhs); }
namespace detail_minlp_solver { NlpModel with_fixed_integers(const NlpModel& nlp,
                             const std::vector<std::size_t>& integer_indices,
                             const std::vector<double>& assignment); }
MinlpSolution iterate_outer_approximation(const MinlpProblem& problem, const NlpModel& nlp,
    const std::vector<double>& x0, const MinlpOptions& options);
MinlpSolution solve_minlp(const MinlpProblem& problem, const std::vector<double>& x0,
                          const MinlpOptions& options);
}
