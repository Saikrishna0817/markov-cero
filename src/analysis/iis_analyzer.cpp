#include "markov_cero/analysis/iis_analyzer.hpp"

#include "markov_cero/api/solve.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <chrono>
#include <sstream>

namespace markov_cero::analysis {

namespace {

bool is_model_infeasible(const model::Model& m, std::size_t& lps_count) {
    ++lps_count;
    api::SolveOptions options;
    options.engine = "simplex";
    options.enable_presolve = false;
    options.enable_scale = true;
    options.lp_options.iteration_limit = 20000;
    
    const auto res = api::solve_model(m, options);
    return res.status == lp::reference::SolveStatus::infeasible;
}

} // namespace

IisResult compute_iis(const model::Model& model) {
    const auto start_time = std::chrono::steady_clock::now();
    IisResult result;
    result.is_infeasible = false;

    // 1. Initial feasibility check
    if (!is_model_infeasible(model, result.lps_solved)) {
        result.diagnostic_summary = "Model is feasible (no IIS exists).";
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        result.analysis_time_ms = std::chrono::duration<double, std::milli>(elapsed).count();
        return result;
    }

    result.is_infeasible = true;
    const std::size_t m = model.matrix.row_count;
    model::Model current_model = model;

    // Active constraints in the candidate set S
    std::vector<bool> in_iis(m, true);

    // 2. Chinneck-Dravnieks Deletion Filter
    for (std::size_t i = 0; i < m; ++i) {
        // Temporarily relax constraint i to (-inf, +inf)
        const auto saved_lower = current_model.row_lower[i];
        const auto saved_upper = current_model.row_upper[i];
        current_model.row_lower[i] = model::Bound::negative_infinity();
        current_model.row_upper[i] = model::Bound::positive_infinity();

        // Check if model without constraint i is still infeasible
        if (is_model_infeasible(current_model, result.lps_solved)) {
            // Still infeasible: constraint i is redundant to the infeasibility
            in_iis[i] = false;
            // keep it relaxed in current_model
        } else {
            // Feasible: constraint i is essential to the conflict
            in_iis[i] = true;
            // restore constraint i
            current_model.row_lower[i] = saved_lower;
            current_model.row_upper[i] = saved_upper;
        }
    }

    // 3. Populate IIS Result
    std::ostringstream oss;
    oss << "Irreducible Infeasible Subsystem contains " ;
    std::size_t count = 0;

    for (std::size_t i = 0; i < m; ++i) {
        if (in_iis[i]) {
            ++count;
            IisConstraint c;
            c.row_index = i;
            c.row_name = (i < model.row_name.size()) ? model.row_name[i] : ("row_" + std::to_string(i));
            c.lower_bound = model.row_lower[i].is_finite() ? model.row_lower[i].value : -1e30;
            c.upper_bound = model.row_upper[i].is_finite() ? model.row_upper[i].value : 1e30;
            
            std::ostringstream desc;
            desc << c.row_name << ": ";
            if (model.row_lower[i].is_finite()) desc << model.row_lower[i].value << " <= ";
            desc << "expr";
            if (model.row_upper[i].is_finite()) desc << " <= " << model.row_upper[i].value;
            c.description = desc.str();

            result.irreducible_subsystem.push_back(std::move(c));
        }
    }

    oss << count << " mutually conflicting constraint(s):\n";
    for (const auto& c : result.irreducible_subsystem) {
        oss << "  - [" << c.row_name << "] " << c.description << "\n";
    }
    result.diagnostic_summary = oss.str();

    const auto elapsed = std::chrono::steady_clock::now() - start_time;
    result.analysis_time_ms = std::chrono::duration<double, std::milli>(elapsed).count();
    return result;
}

} // namespace markov_cero::analysis
