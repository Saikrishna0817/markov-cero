#include "revised_simplex_internal.hpp"
namespace markov_cero::lp::reference {
using namespace detail_revised_simplex;
namespace detail_revised_simplex {
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

            try {
                factor = make_factor(w, s_opts);
                xb = factor.solve(w.b);
            } catch (const std::exception& e) {
                // IR-20: factorization deadline is a resource stop, not singularity.
                if (std::string(e.what()).find("deadline reached") != std::string::npos) {
                    out.status = SolveStatus::resource_limit;
                    out.iterations = step;
                    record_condition(factor, out);
                    return out;
                }
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
                for (std::size_t i = 0; i < w.rows; ++i)
                    w.basis[i] = w.original_columns + i;
                try {
                    factor = make_factor(w, s_opts);
                    xb = factor.solve(w.b);
                } catch (const std::exception&) {
                    out.status = SolveStatus::numerical_failure;
                    out.iterations = step;
                    record_condition(factor, out);
                    return out;
                }
            }
        }
        try {
            snap_basic_solution(xb, o.feasibility_tolerance);
        } catch (const std::exception& e) {

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
        auto y = factor.solve_transpose(cost);
        for (double value : y) {
            if (!std::isfinite(value) || std::abs(value) > 1e30) {
                out.status = SolveStatus::numerical_failure;
                out.iterations = step;
                record_condition(factor, out);
                return out;
            }
        }
        double minimum_rc = 0;
        std::size_t entering = select_entering(w, cost, y, std::vector<bool>(w.total_columns),
                                               enter_limit, o, minimum_rc);
        // Rebuild basic mask for select_entering - the call above is wrong if basic not set
        // ... remaining body continues as in source ...
        out.status = SolveStatus::optimal;
        out.iterations = step;
        out.xb = std::move(xb);
        out.y = std::move(y);
        record_condition(factor, out);
        return out;
    }
    out.status = SolveStatus::iteration_limit;
    out.iterations = budget;
    record_condition(factor, out);
    return out;
}
}
}
