#include "markov_cero/lp/reference/revised_simplex.hpp"

#include "markov_cero/linalg/dense_lu.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <stdexcept>
#include <utility>

namespace markov_cero::lp::reference {
namespace {

constexpr std::size_t maximum_rows = 4096;
constexpr std::size_t maximum_columns = 16384;
constexpr std::size_t maximum_expanded_elements = 64U * 1024U * 1024U;
constexpr std::size_t maximum_iterations = 1000000;
constexpr std::size_t maximum_telemetry = 10000;
constexpr double maximum_tolerance = 1e-4;

std::size_t checked_add(std::size_t a, std::size_t b) {
    if (b > std::numeric_limits<std::size_t>::max() - a)
        throw std::length_error("simplex dimension addition overflow");
    return a + b;
}

std::size_t checked_product(std::size_t a, std::size_t b) {
    if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a)
        throw std::length_error("simplex dimension product overflow");
    const auto n = a * b;
    if (n > maximum_expanded_elements)
        throw std::length_error("simplex dense workspace limit exceeded");
    return n;
}

struct Work {
    std::size_t rows{};
    std::size_t original_rows{};
    std::size_t original_columns{};
    std::size_t total_columns{};
    std::vector<double> a;
    std::vector<double> b;
    std::vector<double> row_sign;
    std::vector<std::size_t> row_origin;
    std::vector<std::size_t> basis;
};

std::vector<double> column(const Work& w, std::size_t j) {
    std::vector<double> v(w.rows);
    for (std::size_t i = 0; i < w.rows; ++i) v[i] = w.a[i * w.total_columns + j];
    return v;
}

linalg::DenseMatrix basis_matrix(const Work& w) {
    linalg::DenseMatrix b{w.rows, w.rows, std::vector<double>(w.rows * w.rows)};
    for (std::size_t j = 0; j < w.rows; ++j) {
        for (std::size_t i = 0; i < w.rows; ++i) {
            b.values[i * w.rows + j] = w.a[i * w.total_columns + w.basis[j]];
        }
    }
    return b;
}

linalg::SparseBasisOptions sparse_options(const Options& o) {
    linalg::SparseBasisOptions so;
    so.singular_tolerance = o.pivot_tolerance;
    so.update_pivot_tolerance = o.pivot_tolerance;
    so.maximum_dimension = maximum_rows;
    so.maximum_nonzeros = maximum_expanded_elements;
    so.maximum_factor_nonzeros = maximum_expanded_elements;
    so.maximum_updates = 16;
    so.eta_density_trigger = 0.9;
    return so;
}

linalg::SparseBasisFactorization make_factor(const Work& w,
                                             const linalg::SparseBasisOptions& opts) {
    std::vector<std::vector<double>> cols;
    cols.reserve(w.rows);
    for (std::size_t j = 0; j < w.rows; ++j) {
        cols.push_back(column(w, w.basis[j]));
    }
    return linalg::SparseBasisFactorization::factorize(
        linalg::SparseCsc::from_columns(w.rows, cols), opts);
}

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    long double s = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        s += static_cast<long double>(a[i]) * b[i];
    }
    const double d = static_cast<double>(s);
    if (!std::isfinite(d)) {
        throw std::overflow_error("non-finite simplex dot product");
    }
    return d;
}

struct IterationOutcome {
    SolveStatus status{SolveStatus::numerical_failure};
    std::vector<double> xb;
    std::vector<double> y;
    std::vector<double> ray;
    std::size_t iterations{};
    double condition_estimate{0.0};
};

// The pivot-ratio condition proxy of the CURRENT basis factorization must be
// recorded BEFORE `return out` (a destructor-based capture would run after the
// return copy and lose the value). current_condition_estimate() folds pending
// eta updates away first, so the proxy matches the basis being returned.
inline void record_condition(linalg::SparseBasisFactorization& factor,
                             IterationOutcome& out) {
    out.condition_estimate = factor.current_condition_estimate();
}

bool significant_negative_reduced_cost(const Work& w, std::size_t j,
                                       const std::vector<double>& cost,
                                       const std::vector<double>& y, double rc, double tol) {
    long double scale = std::abs(static_cast<long double>(cost[j]));
    for (std::size_t i = 0; i < w.rows; ++i) {
        scale += std::abs(static_cast<long double>(w.a[i * w.total_columns + j]) * y[i]);
    }
    const double z = static_cast<double>(scale);
    const double allowed =
        tol * z + 64.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, z);
    return rc < -allowed;
}

void snap_basic_solution(std::vector<double>& xb, double feasibility_tolerance) {
    double max_norm = 1.0;
    for (double v : xb) {
        max_norm = std::max(max_norm, std::abs(v));
    }
    const double tol = std::max({feasibility_tolerance, 1e-6, 1e-8 * max_norm});
    for (double& v : xb) {
        if (v < 0 && v >= -tol) v = 0.0;
        if (v < 0) {
            if (std::getenv("MARKOV_NODE_DEBUG")) {
                std::fprintf(stderr, "[snap_basic_solution] v=%.6e tol=%.6e max_norm=%.6e\n",
                             v, tol, max_norm);
            }
            throw std::runtime_error("primal basis lost feasibility");
        }
    }
}

[[maybe_unused]] std::size_t select_entering(const Work& w, const std::vector<double>& cost,
                                    const std::vector<double>& y, const std::vector<bool>& basic,
                                    std::size_t enter_limit, const Options& o, double& minimum_rc) {
    std::size_t entering = enter_limit;
    minimum_rc = 0;
    for (std::size_t j = 0; j < enter_limit; ++j) {
        if (basic[j]) {
            continue;
        }
        const double rc = cost[j] - dot(column(w, j), y);
        if (!significant_negative_reduced_cost(w, j, cost, y, rc, o.dual_tolerance)) {
            continue;
        }
        if (o.bland_anti_cycling) {
            entering = j;
            minimum_rc = rc;
            break;
        }
        if (entering == enter_limit || rc < minimum_rc) {
            entering = j;
            minimum_rc = rc;
        }
    }
    return entering;
}

std::size_t select_leaving(const Work& w, const std::vector<double>& xb,
                           const std::vector<double>& d, const Options& o, double& theta) {
    std::size_t leaving_row = w.rows;
    theta = std::numeric_limits<double>::infinity();
    const double pivot_thresh = std::max(o.pivot_tolerance, 1e-7);
    for (std::size_t i = 0; i < w.rows; ++i) {
        if (d[i] <= pivot_thresh) {
            continue;
        }
        const double ratio = xb[i] / d[i];
        if (!std::isfinite(ratio)) {
            throw std::overflow_error("non-finite simplex ratio");
        }
        if (ratio < theta ||
            (ratio == theta && (leaving_row == w.rows || w.basis[i] < w.basis[leaving_row]))) {
            theta = ratio;
            leaving_row = i;
        }
    }
    return leaving_row;
}

// EXPAND anti-stalling ratio test (Goldfarb & Reid 1977).
// When degenerate_steps > 0 each row's effective xb is inflated by
//   kExpandEpsilon * degenerate_steps
// so rows that have been degenerate candidates for many consecutive pivots
// are gradually made less competitive in the ratio test.  This breaks long
// degenerate sequences without requiring Bland's strict subscript ordering
// on every pivot.  When degenerate_steps == 0 the result is identical to
// select_leaving above.
static constexpr double kExpandEpsilon = 1e-7;

std::size_t select_leaving_expand(const Work& w, const std::vector<double>& xb,
                                  const std::vector<double>& d, const Options& o,
                                  double& theta, std::size_t degenerate_steps) {
    if (degenerate_steps == 0)
        return select_leaving(w, xb, d, o, theta);

    std::size_t leaving_row = w.rows;
    theta = std::numeric_limits<double>::infinity();
    const double pivot_thresh = std::max(o.pivot_tolerance, 1e-7);
    // Inflated numerator: xb[i] + perturbation so non-degenerate rows
    // (xb[i] >> 0) win the ratio test naturally; perturbation caps at 1e-3
    // to avoid corrupting step-length estimates on large problems.
    const double perturbation =
        std::min(kExpandEpsilon * static_cast<double>(degenerate_steps), 1e-3);

    for (std::size_t i = 0; i < w.rows; ++i) {
        if (d[i] <= pivot_thresh)
            continue;
        const double ratio = (xb[i] + perturbation) / d[i];
        if (!std::isfinite(ratio))
            throw std::overflow_error("non-finite EXPAND ratio");
        if (ratio < theta ||
            (ratio == theta && (leaving_row == w.rows || w.basis[i] < w.basis[leaving_row]))) {
            theta = ratio;
            leaving_row = i;
        }
    }
    // Report true (unperturbed) step length to the pivot.
    if (leaving_row < w.rows && d[leaving_row] > pivot_thresh)
        theta = xb[leaving_row] / d[leaving_row];
    return leaving_row;
}


IterationOutcome iterate(Work& w, const std::vector<double>& cost, std::size_t enter_limit,
                         const Options& o, int phase, std::size_t budget,
                         std::vector<IterationRecord>& log, bool& telemetry_truncated) {
    IterationOutcome out;
    std::vector<char> rejected_column(w.total_columns, 0);
    bool artificial_reset_used = false;
    const auto s_opts = sparse_options(o);
    auto factor = make_factor(w, s_opts);
    std::deque<std::pair<std::size_t, std::size_t>> recent_pivots;
    std::size_t degenerate_steps = 0;
    double last_obj = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t step = 0; step <= budget; ++step) {
        if (o.deadline && std::chrono::steady_clock::now() >= *o.deadline) {
            out.status = SolveStatus::resource_limit;
            out.iterations = step;
            record_condition(factor, out);
            return out;
        }
        if (step == budget) {
            out.status = SolveStatus::iteration_limit;
            out.iterations = step;
            record_condition(factor, out);
            return out;
        }
        auto xb = factor.solve(w.b);
        bool has_neg = false;
        for (double v : xb) {
            if (v < -1e-5) {
                has_neg = true;
                break;
            }
        }
        if (has_neg) {
            if (std::getenv("MARKOV_NODE_DEBUG")) {
                std::fprintf(stderr, "[iterate phase %d step %zu] xb has negative entry; refactorizing basis from scratch\n", phase, step);
            }
            try {
                factor = make_factor(w, s_opts);
                xb = factor.solve(w.b);
            } catch (const std::exception& e) {
                // The accumulated basis itself no longer factorizes: the update
                // chain drifted into numerical singularity. Phase I carries a
                // always-feasible identity fallback (original_columns + i), so
                // reset to it and re-enter phase I from the artificial basis;
                // in phase II report numerical failure honestly.
                if (phase != 1 || artificial_reset_used) {
                    // One reset per phase-I: a second singularity means the
                    // drift is structural, and re-entering from the artificial
                    // basis would only burn the iteration budget in a loop.
                    out.status = SolveStatus::numerical_failure;
                    out.iterations = step;
                    record_condition(factor, out);
                    return out;
                }
                artificial_reset_used = true;
                for (std::size_t i = 0; i < w.rows; ++i) {
                    w.basis[i] = w.original_columns + i;
                }
                factor = make_factor(w, s_opts);
                xb = factor.solve(w.b);
                (void)e;
            }
        }
        try {
            snap_basic_solution(xb, o.feasibility_tolerance);
        } catch (const std::exception& e) {
            if (std::getenv("MARKOV_NODE_DEBUG")) {
                std::fprintf(stderr, "[iterate phase %d step %zu] snap failed after refactorize: %s\n", phase, step, e.what());
            }
            throw;
        }
        for (double value : xb) {
            if (!std::isfinite(value) || std::abs(value) > 1e30) {
                out.status = SolveStatus::numerical_failure;
                out.iterations = step;
                record_condition(factor, out);
                return out;
            }
        }
        std::vector<double> cb(w.rows);
        std::vector<bool> basic(w.total_columns);
        for (std::size_t i = 0; i < w.rows; ++i) {
            cb[i] = cost[w.basis[i]];
            basic[w.basis[i]] = true;
        }
        auto y = factor.solve_transpose(cb);

        const double current_obj = dot(cb, xb);
        if (std::isnan(last_obj) || std::abs(current_obj - last_obj) > 1e-9) {
            degenerate_steps = 0;
            recent_pivots.clear();
        } else {
            ++degenerate_steps;
        }
        last_obj = current_obj;

        const bool use_bland = o.bland_anti_cycling || (degenerate_steps >= 20);

        std::size_t entering = enter_limit;
        std::size_t leaving_row = w.rows;
        std::vector<double> d;
        double theta = 0;
        double minimum_rc = 0;

        std::vector<bool> candidate_tried(enter_limit, false);
        for (std::size_t j = 0; j < enter_limit; ++j) {
            candidate_tried[j] = rejected_column[j] != 0;
        }
        while (true) {
            entering = enter_limit;
            minimum_rc = 0;
            for (std::size_t j = 0; j < enter_limit; ++j) {
                if (basic[j] || candidate_tried[j]) {
                    continue;
                }
                const double rc = cost[j] - dot(column(w, j), y);
                if (!significant_negative_reduced_cost(w, j, cost, y, rc, o.dual_tolerance)) {
                    continue;
                }
                if (use_bland) {
                    entering = j;
                    minimum_rc = rc;
                    break;
                }
                if (entering == enter_limit || rc < minimum_rc) {
                    entering = j;
                    minimum_rc = rc;
                }
            }
            if (entering == enter_limit) {
                out.status = SolveStatus::optimal;
                out.xb = std::move(xb);
                out.y = std::move(y);
                out.iterations = step;
                record_condition(factor, out);
                return out;
            }
            d = factor.solve(column(w, entering));
            leaving_row = select_leaving_expand(w, xb, d, o, theta, degenerate_steps);
            if (leaving_row < w.rows) {
                const auto candidate_leaving = w.basis[leaving_row];
                bool is_cycling = false;
                for (const auto& p : recent_pivots) {
                    if ((p.first == candidate_leaving && p.second == entering) ||
                        (p.first == entering && p.second == candidate_leaving)) {
                        is_cycling = true;
                        break;
                    }
                }
                if (is_cycling && degenerate_steps >= 20) {
                    candidate_tried[entering] = true;
                    continue;
                }
                break;
            }
            if (phase == 1) {
                candidate_tried[entering] = true;
                continue;
            }
            for (double value : d) {
                if (value > 0) {
                    out.status = SolveStatus::numerical_failure;
                    out.iterations = step;
                    record_condition(factor, out);
                    return out;
                }
            }
            out.status = SolveStatus::unbounded;
            out.xb = std::move(xb);
            out.y = std::move(y);
            out.ray.assign(w.total_columns, 0);
            out.ray[entering] = 1;
            for (std::size_t i = 0; i < w.rows; ++i) {
                out.ray[w.basis[i]] = -d[i];
            }
            out.iterations = step;
            record_condition(factor, out);
            return out;
        }
        const auto leaving = w.basis[leaving_row];
        recent_pivots.push_back({entering, leaving});
        if (recent_pivots.size() > 16) {
            recent_pivots.pop_front();
        }
        if (log.size() < o.telemetry_limit) {
            log.push_back({log.size(), phase, dot(cb, xb), minimum_rc, entering, leaving,
                           theta <= o.feasibility_tolerance});
        } else {
            telemetry_truncated = true;
        }
        // Pivot with commit/rollback (numerical hygiene, cf. scsd1/scsd6):
        // the basis swap is only committed once the updated factorization is
        // known to be usable. A pivot whose new basis is numerically singular
        // must never survive: with the swap committed, both the in-place update
        // and the from-scratch rebuild fail identically and the exception
        // escaped the solver as NumericalFailure even though the previous basis
        // was perfectly good. On rejection the basis is restored and the
        // offending column is excluded from entering for the rest of this
        // phase, so the loop makes monotone progress instead of retrying the
        // same pivot forever.
        w.basis[leaving_row] = entering;
        bool pivot_committed = false;
        try {
            factor.replace_column(leaving_row, column(w, entering));
            if (factor.needs_refactorization())
                factor.refactorize();
            pivot_committed = true;
        } catch (const std::exception&) {
            try {
                factor = make_factor(w, s_opts);
                (void)factor.solve(w.b);
                pivot_committed = true;
            } catch (const std::exception&) {
                // New basis is singular: roll back.
            }
        }
        if (!pivot_committed) {
            w.basis[leaving_row] = leaving;
            rejected_column[entering] = 1;
            candidate_tried[entering] = true;
            // Rejecting an entering column can change the dual/basis state;
            // refactorize from the restored basis to be safe.
            try {
                factor = make_factor(w, s_opts);
            } catch (const std::exception&) {
                out.status = SolveStatus::numerical_failure;
                out.iterations = step;
                record_condition(factor, out);
                return out;
            }
        }
    }
    record_condition(factor, out);
    return out;
}

Work make_work(const transform::CanonicalModel& m) {
    Work w;
    w.rows = m.matrix.rows;
    w.original_rows = w.rows;
    w.original_columns = m.matrix.columns;
    if (w.rows > maximum_rows || w.original_columns > maximum_columns) {
        throw std::length_error("simplex reference dimension limit exceeded");
    }
    w.total_columns = checked_add(w.original_columns, w.original_rows);
    const auto work_elements = checked_product(w.rows, w.total_columns);
    checked_product(w.rows, w.rows);
    w.a.assign(work_elements, 0);
    w.b = m.rhs;
    w.row_sign.assign(w.rows, 1);
    w.row_origin.resize(w.rows);
    for (std::size_t i = 0; i < w.rows; ++i) {
        w.row_origin[i] = i;
        if (w.b[i] < 0) {
            w.b[i] = -w.b[i];
            w.row_sign[i] = -1;
        }
        for (std::size_t j = 0; j < w.original_columns; ++j) {
            w.a[i * w.total_columns + j] = w.row_sign[i] * m.matrix(i, j);
        }
        w.a[i * w.total_columns + w.original_columns + i] = 1;
    }
    return w;
}

bool crash_basis(Work& w, double tol, const linalg::SparseBasisOptions& s_opts) {
    w.basis.assign(w.rows, w.total_columns);
    std::vector<bool> used(w.original_columns);
    for (std::size_t i = 0; i < w.rows; ++i) {
        for (std::size_t j = 0; j < w.original_columns; ++j) {
            if (used[j]) {
                continue;
            }
            bool unit = true;
            for (std::size_t r = 0; r < w.rows; ++r) {
                const double expected = r == i ? 1.0 : 0.0;
                if (std::abs(w.a[r * w.total_columns + j] - expected) > tol) {
                    unit = false;
                    break;
                }
            }
            if (unit) {
                w.basis[i] = j;
                used[j] = true;
                break;
            }
        }
        if (w.basis[i] == w.total_columns) {
            return false;
        }
    }
    try {
        auto factor = make_factor(w, s_opts);
        auto xb = factor.solve(w.b);
        for (double v : xb) {
            if (v < -tol) {
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

[[maybe_unused]] void remove_row(Work& w, std::size_t victim) {
    std::vector<double> next;
    next.reserve((w.rows - 1) * w.total_columns);
    for (std::size_t i = 0; i < w.rows; ++i) {
        if (i == victim) {
            continue;
        }
        next.insert(next.end(), w.a.begin() + static_cast<std::ptrdiff_t>(i * w.total_columns),
                    w.a.begin() + static_cast<std::ptrdiff_t>((i + 1) * w.total_columns));
    }
    w.a = std::move(next);
    w.b.erase(w.b.begin() + static_cast<std::ptrdiff_t>(victim));
    w.row_sign.erase(w.row_sign.begin() + static_cast<std::ptrdiff_t>(victim));
    w.row_origin.erase(w.row_origin.begin() + static_cast<std::ptrdiff_t>(victim));
    w.basis.erase(w.basis.begin() + static_cast<std::ptrdiff_t>(victim));
    --w.rows;
}

void remove_artificials(Work& w, double tol, double feas_tol) {
    for (std::size_t i = 0; i < w.rows;) {
        if (w.basis[i] < w.original_columns) {
            ++i;
            continue;
        }
        auto lu = linalg::DenseLu::factorize(basis_matrix(w), tol);
        std::vector<bool> basic(w.total_columns);
        for (auto j : w.basis) {
            basic[j] = true;
        }
        std::size_t entering = w.original_columns;
        for (std::size_t j = 0; j < w.original_columns; ++j) {
            if (basic[j]) {
                continue;
            }
            auto d = lu.solve(column(w, j));
            if (std::abs(d[i]) > tol) {
                // Verify candidate basis preserves primal feasibility
                auto test_basis = w.basis;
                test_basis[i] = j;
                linalg::DenseMatrix test_bm{w.rows, w.rows, std::vector<double>(w.rows * w.rows)};
                for (std::size_t c = 0; c < w.rows; ++c) {
                    for (std::size_t r = 0; r < w.rows; ++r) {
                        test_bm.values[r * w.rows + c] = w.a[r * w.total_columns + test_basis[c]];
                    }
                }
                try {
                    auto test_lu = linalg::DenseLu::factorize(test_bm, tol);
                    auto test_xb = test_lu.solve(w.b);
                    bool feasible = true;
                    for (double val : test_xb) {
                        if (val < -feas_tol) {
                            feasible = false;
                            break;
                        }
                    }
                    if (feasible) {
                        entering = j;
                        break;
                    }
                } catch (...) {
                    continue;
                }
            }
        }
        if (entering < w.original_columns) {
            w.basis[i] = entering;
            ++i;
        } else {
            // Cannot safely replace without violating feasibility;
            // keep artificial in basis with cost 0 (Phase II enter_limit prevents artificial entry).
            ++i;
        }
    }
}

Result certify(const transform::CanonicalModel& m, Result r, double tolerance) {
    if (r.status == SolveStatus::optimal || r.status == SolveStatus::infeasible ||
        r.status == SolveStatus::unbounded) {
        const auto report = verify::verify_reference_result(m, r, tolerance);
        if (!report.accepted) {
            r.status = SolveStatus::numerical_failure;
            r.message = "internal witness verification failed: " + report.message;
        }
    }
    return r;
}

std::vector<double> full_solution(const Work& w, const std::vector<double>& xb) {
    std::vector<double> x(w.total_columns);
    for (std::size_t i = 0; i < w.rows; ++i) x[w.basis[i]] = xb[i];
    return x;
}

bool options_invalid(const Options& o) {
    return o.iteration_limit == 0 || !std::isfinite(o.feasibility_tolerance) ||
           !std::isfinite(o.dual_tolerance) || !std::isfinite(o.pivot_tolerance) ||
           o.feasibility_tolerance <= 0 || o.dual_tolerance <= 0 || o.pivot_tolerance <= 0 ||
           o.feasibility_tolerance > maximum_tolerance || o.dual_tolerance > maximum_tolerance ||
           o.pivot_tolerance > maximum_tolerance || o.pivot_tolerance > o.feasibility_tolerance ||
           o.iteration_limit > maximum_iterations || o.telemetry_limit > maximum_telemetry ||
           std::isnan(o.time_limit_seconds) || o.time_limit_seconds <= 0.0;
}

} // namespace

Result solve_attempt(const transform::CanonicalModel& m, const Options& o) {
    Result result;
    try {
        m.validate();
    } catch (const std::exception& e) {
        result.status = SolveStatus::invalid_model;
        result.message = e.what();
        return result;
    }
    if (options_invalid(o)) {
        result.status = SolveStatus::invalid_options;
        result.message = "invalid simplex options";
        return result;
    }
    try {
        auto w = make_work(m);
        result.telemetry.reserve(std::min(o.iteration_limit, o.telemetry_limit));
        std::vector<double> phase_two_cost(w.total_columns);
        std::copy(m.objective.begin(), m.objective.end(), phase_two_cost.begin());
        std::size_t remaining = o.iteration_limit;
        const auto s_opts = sparse_options(o);
        const bool crashed = crash_basis(w, o.pivot_tolerance, s_opts);
        if (std::getenv("MARKOV_NODE_DEBUG")) {
            std::fprintf(stderr, "[revised_simplex] crashed = %d\n", crashed ? 1 : 0);
        }
        if (!crashed) {
            w.basis.resize(w.rows);
            std::vector<double> phase_one_cost(w.total_columns);
            for (std::size_t i = 0; i < w.rows; ++i) {
                w.basis[i] = w.original_columns + i;
                phase_one_cost[w.basis[i]] = 1;
            }
            auto one = iterate(w, phase_one_cost, w.total_columns, o, 1, remaining,
                               result.telemetry, result.telemetry_truncated);
            result.phase_one_iterations = one.iterations;
            result.condition_estimate = one.condition_estimate;
            remaining -= std::min(remaining, one.iterations);
            if (std::getenv("MARKOV_NODE_DEBUG")) {
                std::fprintf(stderr, "[revised_simplex] phase I done, iters=%zu status=%d\n", one.iterations, (int)one.status);
            }
            if (one.status == SolveStatus::iteration_limit ||
                one.status == SolveStatus::resource_limit) {
                result.status = one.status;
                result.message = one.status == SolveStatus::resource_limit
                                     ? "phase I wall-clock deadline reached"
                                     : "phase I iteration limit";
                return result;
            }
            if (one.status != SolveStatus::optimal) {
                result.status = SolveStatus::numerical_failure;
                result.message = std::string("phase I failed: ") + to_string(one.status);
                return result;
            }
            auto one_x = full_solution(w, one.xb);
            const double phase_one_objective = dot(phase_one_cost, one_x);
            if (phase_one_objective > o.feasibility_tolerance) {
                result.status = SolveStatus::infeasible;
                result.certificate.assign(w.original_rows, 0);
                for (std::size_t i = 0; i < w.rows; ++i) {
                    result.certificate[w.row_origin[i]] = w.row_sign[i] * one.y[i];
                }
                result.message = "validated phase I Farkas candidate";
                return certify(m, std::move(result),
                               std::max(o.feasibility_tolerance, o.dual_tolerance));
            }
            remove_artificials(w, o.pivot_tolerance, o.feasibility_tolerance);
            if (std::getenv("MARKOV_NODE_DEBUG")) {
                std::fprintf(stderr, "[revised_simplex] remove_artificials done\n");
            }
        }
        if (std::getenv("MARKOV_NODE_DEBUG")) {
            std::fprintf(stderr, "[revised_simplex] phase II starting\n");
        }
        auto two = iterate(w, phase_two_cost, w.original_columns, o, 2, remaining, result.telemetry,
                           result.telemetry_truncated);
        result.phase_two_iterations = two.iterations;
        result.condition_estimate = two.condition_estimate;
        result.status = two.status;
        if (two.status == SolveStatus::iteration_limit ||
            two.status == SolveStatus::resource_limit) {
            result.message = two.status == SolveStatus::resource_limit
                                 ? "phase II wall-clock deadline reached"
                                 : "phase II iteration limit";
            return result;
        }
        if (two.status == SolveStatus::unbounded) {
            auto anchor = full_solution(w, two.xb);
            result.primal.assign(anchor.begin(),
                                 anchor.begin() + static_cast<std::ptrdiff_t>(w.original_columns));
            result.ray.assign(two.ray.begin(),
                              two.ray.begin() + static_cast<std::ptrdiff_t>(w.original_columns));
            result.objective = dot(m.objective, result.primal) + m.objective_offset;
            result.message = "primal improving ray";
            return certify(m, std::move(result),
                           std::max(o.feasibility_tolerance, o.dual_tolerance));
        }
        if (two.status != SolveStatus::optimal) {
            result.message = "phase II numerical failure";
            return result;
        }
        auto x = full_solution(w, two.xb);
        result.primal.assign(x.begin(),
                             x.begin() + static_cast<std::ptrdiff_t>(w.original_columns));
        result.dual.assign(w.original_rows, 0);
        for (std::size_t i = 0; i < w.rows; ++i) {
            result.dual[w.row_origin[i]] = w.row_sign[i] * two.y[i];
        }
        result.basis = w.basis;
        result.objective = dot(m.objective, result.primal) + m.objective_offset;
        result.message = "reference primal revised simplex optimum";
        return certify(m, std::move(result), std::max(o.feasibility_tolerance, o.dual_tolerance));
    } catch (const std::length_error& e) {
        if (std::getenv("MARKOV_NODE_DEBUG")) {
            std::fprintf(stderr, "[revised_simplex length_error] %s\n", e.what());
        }
        result.status = SolveStatus::resource_limit;
        result.message = e.what();
        return result;
    } catch (const std::exception& e) {
        if (std::getenv("MARKOV_NODE_DEBUG")) {
            std::fprintf(stderr, "[revised_simplex exception] %s\n", e.what());
        }
        result.status = SolveStatus::numerical_failure;
        result.message = e.what();
        return result;
    }
}

Result solve(const transform::CanonicalModel& m, const Options& o) {
    Options timed = o;
    if (!timed.deadline && std::isfinite(timed.time_limit_seconds) &&
        timed.time_limit_seconds > 0.0) {
        timed.deadline = std::chrono::steady_clock::now() +
                         std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                             std::chrono::duration<double>(timed.time_limit_seconds));
    }
    Result attempt = solve_attempt(m, timed);
    if (attempt.status != SolveStatus::numerical_failure) {
        return attempt;
    }
    // Deterministic single retry: numerical failures on degenerate/scaled
    // models (cf. scsd1/scsd6) come from one unfortunate pivot sequence. A
    // Bland's-rule rerun from the initial basis follows a different, cycle-free
    // pivot order and recovers the optimum on exactly those models. If the
    // retry also fails, the original attempt's message is the honest answer.
    Options retry_options = timed;
    retry_options.bland_anti_cycling = true;
    Result retry = solve_attempt(m, retry_options);
    if (retry.status != SolveStatus::numerical_failure) {
        retry.message += " [recovered via Bland pivot rule]";
        return retry;
    }
    return attempt;
}

const char* to_string(SolveStatus s) noexcept {
    switch (s) {
    case SolveStatus::optimal: return "Optimal";
    case SolveStatus::infeasible: return "Infeasible";
    case SolveStatus::unbounded: return "Unbounded";
    case SolveStatus::iteration_limit: return "IterationLimit";
    case SolveStatus::invalid_model: return "InvalidModel";
    case SolveStatus::invalid_options: return "InvalidOptions";
    case SolveStatus::resource_limit: return "ResourceLimit";
    case SolveStatus::numerical_failure: return "NumericalFailure";
    case SolveStatus::unsupported: return "Unsupported";
    case SolveStatus::non_convex_minlp: return "NonConvexMINLP";
    }
    return "Unknown";
}

} // namespace markov_cero::lp::reference
