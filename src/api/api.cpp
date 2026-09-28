#include "api_internal.hpp"

namespace markov_cero::api::detail {
SolveOptions with_api_deadline(SolveOptions options, Clock::time_point started) {
    auto deadline = options.lp_options.deadline;
    const auto include = [&](std::optional<Clock::time_point> candidate) {
        if (candidate && (!deadline || *candidate < *deadline)) deadline = candidate;
    };
    const auto add_limit = [&](double seconds) {
        if (!std::isfinite(seconds) || seconds <= 0) return;
        const auto remaining = Clock::time_point::max() - started;
        if (seconds < std::chrono::duration<double>(remaining).count())
            include(started + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds)));
    };
    include(options.milp_options.deadline);
    add_limit(options.lp_options.time_limit_seconds);
    // The MIP duration applies only when an integer model is dispatched.
    options.lp_options.deadline = deadline;
    return options;
}

double elapsed_ms(Clock::time_point started) {
    return std::chrono::duration<double, std::milli>(Clock::now() - started).count();
}

std::string format_violation(const verify::PrimalVerificationReport& report) {
    if (report.violations.empty()) return "";
    const auto& v = report.violations[0];
    return v.category + " idx=" + std::to_string(v.index) +
           " act=" + std::to_string(v.actual) +
           " bnd=" + std::to_string(v.bound) +
           " diff=" + std::to_string(v.magnitude) +
           " allow=" + std::to_string(v.allowance);
}

// C-3: complementarity gap |c^T x - b^T y| of a primal/dual pair. The row
// bound vector b carries explicit rhs values (canonical models); callers that
// only have ranged row bounds pass the bound complementary to sign(y).
void fill_complementarity_gap(NumericalDiagnostic& diag, const std::vector<double>& c,
                              const std::vector<double>& b, const std::vector<double>& x,
                              const std::vector<double>& y) {
    if (x.size() != c.size() || y.size() != b.size()) return;
    long double primal = 0.0L;
    long double dual = 0.0L;
    for (std::size_t j = 0; j < x.size(); ++j) {
        primal += static_cast<long double>(c[j]) * x[j];
    }
    for (std::size_t i = 0; i < y.size(); ++i) {
        dual += static_cast<long double>(b[i]) * y[i];
    }
    const double gap = std::abs(static_cast<double>(primal - dual));
    if (std::isfinite(gap)) {
        diag.complementarity_gap = gap;
    }
}

void sync_engine_result(SolveResult& out, const lp::reference::Result& result) {
    out.status = result.status;
    out.message = result.message;
    out.primal = result.primal;
    out.objective = result.objective;
    out.phase_one_iterations = result.phase_one_iterations;
    out.phase_two_iterations = result.phase_two_iterations;
    // LP engines carry their basis / normal-matrix condition proxy in the
    // reference result. Engines that already filled out.diagnostic directly
    // (PDLP crossover, QP, MILP) keep their value.
    if (out.diagnostic.condition_estimate == 0.0 && result.condition_estimate > 0.0) {
        out.diagnostic.condition_estimate = result.condition_estimate;
    }
}


template <class Body>
void guarded(SolveResult& out, lp::reference::Result& result, Body&& body) {
    try {
        body();
    } catch (const io::MpsError& e) {
        result.status = lp::reference::SolveStatus::invalid_model;
        result.message = e.what();
        out.error = e.what();
        out.diagnostic.failure_site = "mps_parser";
        out.diagnostic.suggested_recovery = "correct_mps_syntax_at_indicated_record";
    } catch (const io::MpsResourceLimitError& e) {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.message = e.what();
        out.error = e.what();
        out.diagnostic.failure_site = "input_resource_limit";
        out.diagnostic.suggested_recovery = "reduce_file_or_configured_parser_limits";
    } catch (const io::LpResourceLimitError& e) {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.message = e.what();
        out.error = e.what();
        out.diagnostic.failure_site = "input_resource_limit";
        out.diagnostic.suggested_recovery = "reduce_file_or_configured_parser_limits";
    } catch (const std::invalid_argument& e) {
        result.status = lp::reference::SolveStatus::invalid_model;
        result.message = e.what();
        out.error = e.what();
        out.diagnostic.failure_site = "input_validation";
        out.diagnostic.suggested_recovery = "verify_model_dimensions_and_bounds";
    } catch (const std::length_error& e) {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.message = e.what();
        out.error = e.what();
        out.diagnostic.failure_site = "memory_or_factor_limit";
        out.diagnostic.suggested_recovery = "increase_maximum_factor_nonzeros";
    } catch (const std::exception& e) {
        result.status = lp::reference::SolveStatus::numerical_failure;
        result.message = e.what();
        out.error = e.what();
        out.diagnostic.failure_site = "unhandled_exception";
        out.diagnostic.suggested_recovery = "inspect_runtime_error_message";
    }
    sync_engine_result(out, result);
}

void finalize(SolveResult& out) {
    out.verified =
        (out.status == lp::reference::SolveStatus::optimal && out.original_verified &&
         out.canonical_verified) ||
        ((out.status == lp::reference::SolveStatus::infeasible ||
          out.status == lp::reference::SolveStatus::unbounded) &&
         out.canonical_verified);

    // Eliminate silent failures: guarantee valid diagnostic state
    if (out.diagnostic.failure_site.empty()) {
        if (out.status == lp::reference::SolveStatus::optimal) {
            out.diagnostic.failure_site = "none";
            out.diagnostic.suggested_recovery = "none";
        } else {
            out.diagnostic.failure_site = out.resolved_engine + "_solve";
            out.diagnostic.suggested_recovery = "inspect_engine_numerics_and_parameters";
        }
    }
    if (out.diagnostic.primal_residual == 0.0) {
        out.diagnostic.primal_residual = std::max(out.canonical_report.maximum_primal_violation,
                                                 out.primal_report.maximum_row_violation);
    }
    if (out.diagnostic.dual_residual == 0.0) {
        out.diagnostic.dual_residual = out.canonical_report.maximum_dual_violation;
    }
    // condition_estimate is engine-populated: 0.0 here means the resolved
    // engine performed no factorization (matrix-free PDLP / SQP), which is
    // reported as-is rather than replaced by a fake "perfectly conditioned".
}


} // namespace markov_cero::api::detail

namespace markov_cero::api {
using namespace detail;
SolveResult solve_file(const std::string& path, const SolveOptions& options) {
    const auto started = Clock::now();
    const auto timed_options = with_api_deadline(options, started);
    SolveResult out;
    out.resolved_engine = options.engine;
    if (options.maximum_input_bytes &&
        (*options.maximum_input_bytes == 0 || *options.maximum_input_bytes > 1073741824ULL)) {
        out.status = lp::reference::SolveStatus::invalid_options;
        out.message = "maximum_input_bytes must be in 1..1073741824";
        out.error = out.message;
        out.diagnostic.failure_site = "input_resource_budget";
        out.runtime_ms = elapsed_ms(started);
        return out;
    }
    std::ifstream input(path);
    if (!input) {
        out.status = lp::reference::SolveStatus::invalid_model;
        out.message = "cannot open input";
        out.error = "cannot open input";
        out.input_open_failed = true;
        out.diagnostic.failure_site = "file_io";
        out.diagnostic.suggested_recovery = "verify_file_exists_and_has_read_permissions";
        out.runtime_ms = elapsed_ms(started);
        return out;
    }
    lp::reference::Result result;
    guarded(out, result, [&] {
        const bool is_lp = (path.size() >= 3 &&
            (path.rfind(".lp") == path.size() - 3 || path.rfind(".LP") == path.size() - 3));
        // R12 (thousands-to-millions of nonzeros): the default 16 MB parser
        // byte limit rejects ~800k-nnz models outright, so file parsing runs
        // with an explicitly raised limit. All other structural limits are
        // unchanged.
        io::MpsLimits limits;
        limits.maximum_bytes = 256U * 1024U * 1024U;
        if (options.maximum_input_bytes) limits.maximum_bytes = *options.maximum_input_bytes;
        io::LpLimits lp_limits;
        if (options.maximum_input_bytes)
            lp_limits.maximum_bytes = *options.maximum_input_bytes;
        const auto model = is_lp ? io::parse_lp_file(path, lp_limits)
                                 : io::parse_mps(input, limits);
        run_engine(model, timed_options, out, result);
    });
    finalize(out);
    out.runtime_ms = elapsed_ms(started);
    return out;
}

SolveResult solve_model(const model::Model& model, const SolveOptions& options) {
    const auto started = Clock::now();
    const auto timed_options = with_api_deadline(options, started);
    SolveResult out;
    out.resolved_engine = options.engine;
    lp::reference::Result result;
    guarded(out, result, [&] { run_engine(model, timed_options, out, result); });
    finalize(out);
    out.runtime_ms = elapsed_ms(started);
    return out;
}

} // namespace markov_cero::api
