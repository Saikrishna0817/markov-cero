#pragma once
#include "markov_cero/lp/reference/revised_simplex.hpp"

#include "markov_cero/linalg/dense_lu.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace markov_cero::lp::reference {
namespace detail_revised_simplex {}
namespace detail_revised_simplex {
constexpr std::size_t maximum_rows = 4096;
}
namespace detail_revised_simplex {
constexpr std::size_t maximum_columns = 16384;
}
namespace detail_revised_simplex {
constexpr std::size_t maximum_expanded_elements = 64U * 1024U * 1024U;
}
namespace detail_revised_simplex {
constexpr std::size_t maximum_iterations = 1000000;
}
namespace detail_revised_simplex {
constexpr std::size_t maximum_telemetry = 10000;
}
namespace detail_revised_simplex {
constexpr double maximum_tolerance = 1e-4;
}
namespace detail_revised_simplex {
struct Work {
    std::size_t rows{};
    std::size_t original_rows{};
    std::size_t original_columns{};
    std::size_t total_columns{};
    std::vector<std::vector<std::pair<std::size_t, double>>> a;
    std::vector<double> b;
    std::vector<double> row_sign;
    std::vector<std::size_t> row_origin;
    std::vector<std::size_t> basis;
};
}
namespace detail_revised_simplex {
struct IterationOutcome {
    SolveStatus status{SolveStatus::numerical_failure};
    std::vector<double> xb;
    std::vector<double> y;
    std::vector<double> ray;
    std::size_t iterations{};
    double condition_estimate{0.0};
};
}
namespace detail_revised_simplex { std::size_t checked_add(std::size_t a, std::size_t b); }
namespace detail_revised_simplex { std::vector<double> column(const Work& w, std::size_t j); }
namespace detail_revised_simplex { double column_dot(const Work& w, std::size_t j, const std::vector<double>& y); }
namespace detail_revised_simplex { linalg::SparseBasisOptions sparse_options(const Options& o); }
namespace detail_revised_simplex { linalg::SparseBasisFactorization make_factor(const Work& w,
                                             const linalg::SparseBasisOptions& opts); }
namespace detail_revised_simplex { double dot(const std::vector<double>& a, const std::vector<double>& b); }
namespace detail_revised_simplex { void record_condition(linalg::SparseBasisFactorization& factor,
                             IterationOutcome& out); }
namespace detail_revised_simplex { bool significant_negative_reduced_cost(const Work& w, std::size_t j,
                                       const std::vector<double>& cost,
                                       const std::vector<double>& y, double rc, double tol); }
namespace detail_revised_simplex { void snap_basic_solution(std::vector<double>& xb, double feasibility_tolerance); }
namespace detail_revised_simplex { std::size_t select_entering(const Work& w, const std::vector<double>& cost,
                                    const std::vector<double>& y, const std::vector<bool>& basic,
                                    std::size_t enter_limit, const Options& o, double& minimum_rc); }
namespace detail_revised_simplex { std::size_t select_leaving(const Work& w, the std::vector<double>& xb,
                           const std::vector<double>& d, const Options& o, double& theta); }
namespace detail_revised_simplex { IterationOutcome iterate(Work& w, const std::vector<double>& cost, std::size_t enter_limit,
                         const Options& o, int phase, std::size_t budget,
                         std::vector<IterationRecord>& log, bool& telemetry_truncated); }
namespace detail_revised_simplex { Work make_work(const transform::SparseCanonicalModel& m); }
namespace detail_revised_simplex { bool crash_basis(Work& w, double tol, const linalg::SparseBasisOptions& s_opts); }
namespace detail_revised_simplex { void remove_row(Work& w, std::size_t victim); }
namespace detail_revised_simplex { void remove_artificials(Work& w, double tol, double feas_tol); }
namespace detail_revised_simplex { Result certify(const transform::SparseCanonicalModel& m, Result r, double tolerance); }
namespace detail_revised_simplex { std::vector<double> full_solution(const Work& w, const std::vector<double>& xb); }
namespace detail_revised_simplex { bool options_invalid(const Options& o); }
Result solve_attempt(const transform::SparseCanonicalModel& m, const Options& o);
Result solve(const transform::SparseCanonicalModel& m, const Options& o);
Result solve(const transform::CanonicalModel& model, const Options& options);
const char* to_string(SolveStatus s) noexcept;
}
