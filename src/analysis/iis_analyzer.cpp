#include "markov_cero/analysis/iis_analyzer.hpp"
#include "markov_cero/api/solve.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace markov_cero::analysis {
IisResult compute_iis(const model::Model& model, const IisOptions& settings) {
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    IisResult result;
    auto finish = [&](const std::string& message) {
        result.diagnostic_summary = message;
        result.analysis_time_ms = std::chrono::duration<double, std::milli>(Clock::now()-started).count();
        return result;
    };
    if (!std::isfinite(settings.time_limit_seconds) || settings.time_limit_seconds <= 0 || settings.time_limit_seconds > 1e8)
        return finish("Invalid conflict-analysis time budget.");
    if (model.has_quadratic_objective || model.has_nlobj_section || model.nlp_callbacks ||
        std::any_of(model.variable_type.begin(), model.variable_type.end(),
                    [](auto type) { return type != model::VariableType::continuous; }))
        return finish("Unsupported: conflict analysis accepts continuous linear models only.");
    auto deadline = started + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(settings.time_limit_seconds));
    if (settings.deadline && *settings.deadline < deadline) deadline = *settings.deadline;
    auto check = [&](const model::Model& candidate) {
        ++result.lps_solved;
        api::SolveOptions options;
        options.engine = "primal";
        options.enable_presolve = false;
        options.lp_options.iteration_limit = 20000;
        options.lp_options.deadline = deadline;
        return api::solve_model(candidate, options);
    };
    const auto initial = check(model);
    if (initial.status != lp::reference::SolveStatus::infeasible || !initial.canonical_verified)
        return finish(initial.verified ? "Model has a certified feasible solution; no conflict exists."
                                       : "Inconclusive: initial infeasibility was not certified.");
    result.is_infeasible = true;
    result.complete = true;
    auto current = model;
    std::vector<bool> keep(model.matrix.row_count, true);
    for (std::size_t i=0; i<keep.size(); ++i) {
        if (Clock::now() >= deadline) { result.complete = false; break; }
        const auto lower = current.row_lower[i], upper = current.row_upper[i];
        current.row_lower[i] = model::Bound::negative_infinity();
        current.row_upper[i] = model::Bound::positive_infinity();
        const auto trial = check(current);
        if (trial.status == lp::reference::SolveStatus::infeasible && trial.canonical_verified) keep[i] = false;
        else {
            current.row_lower[i] = lower; current.row_upper[i] = upper;
            // Unknown trials do not establish that a row is essential.
            if (!trial.original_verified) result.complete = false;
        }
    }
    std::ostringstream text;
    text << (result.complete ? "Row-irreducible" : "Incomplete")
         << " certified infeasible subsystem, relative to unchanged variable bounds:\n";
    for (std::size_t i=0; i<keep.size(); ++i) {
        if (!keep[i]) continue;
        IisConstraint row;
        row.row_index = i; row.row_name = model.row_name[i];
        row.lower_bound = model.row_lower[i].is_finite() ? model.row_lower[i].value : -INFINITY;
        row.upper_bound = model.row_upper[i].is_finite() ? model.row_upper[i].value : INFINITY;
        std::ostringstream description;
        description << row.row_name << ": " << row.lower_bound << " <= activity <= " << row.upper_bound;
        row.description = description.str(); text << "  " << row.description << '\n';
        result.irreducible_subsystem.push_back(std::move(row));
    }
    return finish(text.str());
}
} // namespace markov_cero::analysis
