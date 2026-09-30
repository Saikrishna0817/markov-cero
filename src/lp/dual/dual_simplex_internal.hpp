#pragma once
#include "markov_cero/lp/dual/dual_simplex.hpp"

#include "markov_cero/linalg/dense_lu.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace markov_cero::lp::dual {
namespace detail_dual_simplex {}
namespace detail_dual_simplex {
constexpr std::size_t maximum_rows = 4096;
}
namespace detail_dual_simplex {
constexpr std::size_t maximum_columns = 16384;
}
namespace detail_dual_simplex {
constexpr std::size_t maximum_iterations = 1000000;
}
namespace detail_dual_simplex {
constexpr std::size_t maximum_telemetry = 10000;
}
namespace detail_dual_simplex {
constexpr std::size_t maximum_dense_elements = 64U * 1024U * 1024U;
}
namespace detail_dual_simplex {
constexpr double maximum_tolerance = 1e-4;
}
namespace detail_dual_simplex { std::uint64_t mix(std::uint64_t h, std::uint64_t v); }
namespace detail_dual_simplex { std::string hex(std::uint64_t h); }
namespace detail_dual_simplex { std::uint64_t hash_text(const std::string& s); }
namespace detail_dual_simplex { void check_product(std::size_t a, std::size_t b); }
namespace detail_dual_simplex { double dot(const std::vector<double>& a, const std::vector<double>& b); }
namespace detail_dual_simplex { linalg::SparseCsc sparse_basis_matrix(const transform::SparseCanonicalModel& m,
                                      const std::vector<std::size_t>& basis); }
namespace detail_dual_simplex { linalg::SparseBasisOptions sparse_options(const Options& o); }
namespace detail_dual_simplex { bool significant_negative_reduced_cost(const transform::SparseCanonicalModel& m, std::size_t j,
                                       const std::vector<double>& y, double rc, double tol); }
namespace detail_dual_simplex { std::vector<double> full_primal(std::size_t n, const std::vector<std::size_t>& basis,
                                const std::vector<double>& xb); }
namespace detail_dual_simplex { void validate_options(const Options& o); }
namespace detail_dual_simplex { void validate_basis_metadata(const transform::SparseCanonicalModel& m, const BasisState& s); }
namespace detail_dual_simplex { void validate_basis(const transform::SparseCanonicalModel& m, const BasisState& s); }
namespace detail_dual_simplex { Result cold(const transform::SparseCanonicalModel& m, const Options& o, const std::string& why); }
namespace detail_dual_simplex { std::vector<double> compute_exact_dse_weights(const transform::SparseCanonicalModel& m,
                                              linalg::SparseBasisFactorization& factor); }
namespace detail_dual_simplex { std::size_t select_leaving_row(const transform::SparseCanonicalModel& m,
                               linalg::SparseBasisFactorization& factor,
                               const std::vector<double>& xb, const std::vector<std::size_t>& basis,
                               const std::vector<double>& dse_weights,
                               const Options& o, double& worst); }
namespace detail_dual_simplex { std::size_t select_entering_column(const transform::SparseCanonicalModel& m,
                                   const std::vector<double>& alpha, const std::vector<double>& rc,
                                   const std::vector<bool>& is_basic, const Options& o,
                                   double& best_ratio); }
namespace detail_dual_simplex { Result certified_optimal(const transform::SparseCanonicalModel& m, const std::vector<std::size_t>& basis,
                         const std::vector<double>& xb, const std::vector<double>& y,
                         const Options& o); }
namespace detail_dual_simplex { Result certified_farkas(const transform::SparseCanonicalModel& m, const std::vector<double>& pi,
                        const Options& o); }
namespace detail_dual_simplex { bool accepted_status(reference::SolveStatus status); }
namespace detail_dual_simplex { Result verify_accepted(const transform::SparseCanonicalModel& m, const Options& o,
                          Result out); }
std::string fingerprint(const transform::SparseCanonicalModel& m);
BasisState make_basis_state(const transform::SparseCanonicalModel& m, const std::vector<std::size_t>& b);
void validate_basis_artifact(const BasisState& s);
std::string serialize_basis(const BasisState& s);
BasisState parse_basis(const std::string& text);
Result solve(const transform::SparseCanonicalModel& m, const Options& o,
             const std::optional<BasisState>& warm);
Result solve_impl(const transform::SparseCanonicalModel& m, const Options& o,
                  const std::optional<BasisState>& warm, FactorCache* cache);
Result solve_verified(const transform::SparseCanonicalModel& m, const Options& o,
                      const std::optional<BasisState>& warm, FactorCache* cache);
}
