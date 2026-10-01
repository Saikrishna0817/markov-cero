#pragma once
#include "markov_cero/minlp/minlp_solver.hpp"

#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
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
namespace detail_minlp_solver {
// MINLP-01 (minlp-oa.md §4-§5): accumulated master rows plus the provenance
// record of every stored tangent.
struct OaRowAccumulators {
    std::vector<std::vector<double>> obj_grads;
    std::vector<double> obj_rhs;   // grad f(x^k)^T x^k - f(x^k) + weakening
    std::vector<std::vector<double>> cut_grads;
    std::vector<double> cut_rhs;   // J_i(x^k)^T x^k - g_i(x^k) + weakening
    std::vector<OaCut> cuts;
};
// Append objective + constraint cuts at p and replay the new cuts against the
// source polynomial; empty string on success, else the fail-closed message.
std::string append_oa_rows(const model::Model& source, const NlpModel& nlp,
                           const std::vector<OaRowSource>& row_map,
                           const std::vector<double>& p, double f_at_p,
                           OaRowAccumulators& acc, std::size_t& cuts_replayed);
// Contract §7.6: every incumbent-retaining exit carries x, objective and the
// last certified bound (gap when both bounds are finite).
void retain_incumbent(MinlpSolution& out, const std::vector<double>& best_x, double best_obj,
                      double best_bound);
}
MinlpSolution iterate_outer_approximation(const MinlpProblem& problem, const NlpModel& nlp,
    const std::vector<double>& x0, const MinlpOptions& options);
MinlpSolution solve_minlp(const MinlpProblem& problem, const std::vector<double>& x0,
                          const MinlpOptions& options);
}
