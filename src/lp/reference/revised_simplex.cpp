#include "revised_simplex_internal.hpp"
namespace markov_cero::lp::reference {
using namespace detail_revised_simplex;
namespace detail_revised_simplex {
Result certify(const transform::SparseCanonicalModel& m, Result r, double tolerance) {
    if (r.status == SolveStatus::optimal || r.status == SolveStatus::infeasible ||
        r.status == SolveStatus::unbounded) {
        const auto report = verify::verify_sparse_result(m, r, tolerance);
        if (!report.accepted) {
            r.status = SolveStatus::numerical_failure;
            r.message = "internal witness verification failed: " + report.message;
        }
    }
    return r;
}
}

namespace detail_revised_simplex {
std::vector<double> full_solution(const Work& w, const std::vector<double>& xb) {
    std::vector<double> x(w.total_columns);
    for (std::size_t i = 0; i < w.rows; ++i) x[w.basis[i]] = xb[i];
    return x;
}
}

namespace detail_revised_simplex {
bool options_invalid(const Options& o) {
    return o.iteration_limit == 0 || !std::isfinite(o.feasibility_tolerance) ||
           !std::isfinite(o.dual_tolerance) || !std::isfinite(o.pivot_tolerance) ||
           o.feasibility_tolerance <= 0 || o.dual_tolerance <= 0 || o.pivot_tolerance <= 0 ||
           o.feasibility_tolerance > maximum_tolerance || o.dual_tolerance > maximum_tolerance ||
           o.pivot_tolerance > maximum_tolerance || o.pivot_tolerance > o.feasibility_tolerance ||
           o.iteration_limit > maximum_iterations || o.telemetry_limit > maximum_telemetry ||
           std::isnan(o.time_limit_seconds) || o.time_limit_seconds <= 0.0;
}
}

Result solve_attempt(const transform::SparseCanonicalModel& m, const Options& o,
                     const std::vector<std::size_t>& warm_basis) {
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
    if (o.deadline && std::chrono::steady_clock::now() >= *o.deadline) {
        result.status = SolveStatus::resource_limit;
        result.message = "simplex wall-clock deadline reached before factorization";
        return result;
    }
    try {
        auto w = make_work(m);
        result.telemetry.reserve(std::min(o.iteration_limit, o.telemetry_limit));
        std::vector<double> phase_two_cost(w.total_columns);
        std::copy(m.objective.begin(), m.objective.end(), phase_two_cost.begin());
        std::size_t remaining = o.iteration_limit;
        const auto s_opts = sparse_options(o);
        // Warm start (crossover handoff): accept the supplied basis only when
        // it factorizes and its basic solution is primal feasible; anything
        // else discards it and falls through to crash_basis() exactly as the
        // two-argument solve() would.
        bool warm_accepted = false;
        if (!warm_basis.empty()) {
            try {
                if (warm_basis.size() != w.rows)
                    throw std::invalid_argument("warm basis dimension mismatch");
                std::vector<char> seen(w.total_columns, 0);
                for (std::size_t j : warm_basis) {
                    if (j >= w.total_columns || seen[j])
                        throw std::invalid_argument("warm basis column invalid or duplicated");
                    seen[j] = 1;
                }
                w.basis.assign(warm_basis.begin(), warm_basis.end());
                auto warm_factor = make_factor(w, s_opts);
                auto warm_x = warm_factor.solve(w.b);
                snap_basic_solution(warm_x, o.feasibility_tolerance);
                warm_accepted = true;
            } catch (const std::exception&) {
                warm_accepted = false;
            }
        }
        const bool crashed =
            warm_accepted || crash_basis(w, o.pivot_tolerance, s_opts);

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
                result.message = std::string("phase I failed: ") +
                                 (one.message.empty() ? to_string(one.status) : one.message);
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
            result.message = std::string("phase II numerical failure") +
                             (two.message.empty() ? "" : ": " + two.message);
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

        result.status = SolveStatus::resource_limit;
        result.message = e.what();
        return result;
    } catch (const std::exception& e) {

        result.status = o.deadline && std::chrono::steady_clock::now() >= *o.deadline
                            ? SolveStatus::resource_limit : SolveStatus::numerical_failure;
        result.message = e.what();
        return result;
    }
}
Result solve(const transform::SparseCanonicalModel& m, const Options& o,
             const std::vector<std::size_t>& warm_basis) {
    Options timed = o;
    if (!timed.deadline && std::isfinite(timed.time_limit_seconds) &&
        timed.time_limit_seconds > 0.0) {
        timed.deadline = std::chrono::steady_clock::now() +
                         std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                             std::chrono::duration<double>(timed.time_limit_seconds));
    }
    Result attempt = solve_attempt(m, timed, warm_basis);
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
    Result retry = solve_attempt(m, retry_options, warm_basis);
    if (retry.status != SolveStatus::numerical_failure) {
        // IR-21: only a completed retry recovered — a resource stop (a
        // refused factor charge) must not wear a recovery label.
        if (retry.status != SolveStatus::resource_limit) {
            retry.message += " [recovered via Bland pivot rule]";
        }
        return retry;
    }
    return attempt;
}
Result solve(const transform::SparseCanonicalModel& m, const Options& o) {
    return solve(m, o, std::vector<std::size_t>{});
}
Result solve(const transform::CanonicalModel& model, const Options& options) {
    // Dense adapter (contract docs/contracts/sparse-lp-path.md §1): one shared
    // conversion, then the identical sparse solve.
    return solve(transform::sparse_from_dense(model), options);
}
const char* to_string(SolveStatus s) noexcept {
    switch (s) {
    case SolveStatus::gap_satisfied: return "GapSatisfied";
    case SolveStatus::local_optimal: return "LocalStationary";
    case SolveStatus::feasible: return "Feasible";
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
}
