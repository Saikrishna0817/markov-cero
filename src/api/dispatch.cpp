#include "api_internal.hpp"

namespace markov_cero::api::detail {
void run_engine(const model::Model& model, const SolveOptions& input_options, SolveResult& out,
                lp::reference::Result& result) {
    auto options = input_options;
    if (options.engine == "simplex" || options.engine == "primal_simplex") { options.engine = "primal"; out.resolved_engine = "primal"; }
    const std::string engines[] = {"auto", "primal", "dual", "ipm", "pdlp", "qp", "milp", "miqp", "parallel", "sqp", "outer_approx"};
    if (std::find(std::begin(engines), std::end(engines), options.engine) == std::end(engines) ||
        (options.backend != "cpu" && options.backend != "gpu") || options.num_threads == 0 || options.num_threads > 256 ||
        !std::isfinite(options.pdlp_tolerance) || options.pdlp_tolerance <= 0 ||
        !std::isfinite(options.mip_proof_time_limit_seconds) ||
        options.mip_proof_time_limit_seconds <= 0 || options.mip_proof_time_limit_seconds > 1e8 ||
        options.mip_proof_max_nodes == 0 || options.mip_proof_max_nodes > 100000 ||
        options.mip_proof_max_witness_values == 0 ||
        options.mip_proof_max_witness_values > 16000000 ||
        (options.maximum_input_bytes &&
         (*options.maximum_input_bytes == 0 || *options.maximum_input_bytes > 1073741824ULL))) {
        result.status = lp::reference::SolveStatus::invalid_options;
        result.message = "invalid engine, backend, worker count, tolerance or resource budget"; return;
    }
    const bool integer = std::any_of(model.variable_type.begin(), model.variable_type.end(),
        [](auto type) { return type != model::VariableType::continuous; });
    if (integer && std::isfinite(options.milp_options.time_limit_seconds) &&
        options.milp_options.time_limit_seconds > 0 && options.milp_options.time_limit_seconds < 1e8) {
        const auto end = Clock::now() + std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(options.milp_options.time_limit_seconds));
        if (!options.lp_options.deadline || end < *options.lp_options.deadline) options.lp_options.deadline = end;
        options.milp_options.deadline = options.lp_options.deadline;
    }
    if (stop_after_deadline(options, out, result, "input parsing or before engine dispatch")) return;
    model.validate();
    out.variable_names = model.variable_name;
    out.row_names = model.row_name;
    out.model_rows = model.matrix.row_count;
    out.model_cols = model.matrix.column_count;
    out.model_nnz = model.matrix.value.size();

    // W6: classify every model with the locked decision tree and record the
    // outcome in the result telemetry. Auto dispatch now flows through the
    // classifier's locked engine-selection table. W1 Path B: an NLOBJ section
    // present in the parsed file feeds the tree's NLP branch. W1 Path A: an
    // attached programmatic callback companion (model.nlp_callbacks) feeds
    // the tree's first branch (callback presence).
    model::ClassificationInputs classification_inputs;
    classification_inputs.has_nlobj_section = model.has_nlobj_section;
    classification_inputs.has_nlp_callbacks = model.nlp_callbacks.has_value();
    const auto classification = model::classify_model(model, classification_inputs);
    const auto stats = model::classify_stats(model);
    out.problem_class = model::to_string(classification.problem_class);
    out.classification_reason = classification.reason;

    if (out.resolved_engine == "auto") {
        out.resolved_engine = model::select_engine(
            classification.problem_class, stats, options.backend == "gpu");
        // MILP honors an explicit multi-thread request through the parallel
        // engine (locked rule table, plan W6). The compiled-in thread default
        // alone must not change dispatch — only an explicit --threads request
        // (or programmatic equivalent) triggers the upgrade.
        if (classification.problem_class == model::ProblemClass::milp &&
            options.num_threads > 1 && options.threads_explicit &&
            options.milp_options.branching_strategy != milp::BranchingStrategy::ml_gnn) {
            out.resolved_engine = "parallel";
        }
        // Locked GPU backend gating for auto dispatch: GPU is only recommended
        // above the size thresholds and only when the caller asked for it.
        if (options.backend == "gpu" &&
            ((out.resolved_engine == "pdlp" &&
              stats.nonzeros > model::EngineThresholds::kPdlpGpuNnzThreshold) ||
             (out.resolved_engine == "qp" &&
              stats.quadratic_nonzeros > model::EngineThresholds::kQpGpuNnzThreshold))) {
            out.recommended_backend = "gpu";
        }
    } else {
        // Explicit engine requests keep the caller's backend choice; the
        // thresholds gate auto-dispatch only (locked rule table).
        out.recommended_backend = options.backend;
    }

    // The parallel tree engine does not yet own an ML scorer per worker. Keep
    // an explicit ml_gnn request on the serial MILP path instead of silently
    // routing it through a pseudo-cost-only implementation.
    if (out.resolved_engine == "parallel" &&
        options.milp_options.branching_strategy == milp::BranchingStrategy::ml_gnn) {
        out.resolved_engine = "milp";
        out.classification_reason +=
            "; ml_gnn uses the serial MILP engine until parallel scorer wiring is available";
    }

    const bool nonlinear = model.has_nlobj_section || !model.nlcon_constraints.empty() || model.nlp_callbacks.has_value();
    const bool linear_engine = out.resolved_engine == "primal" || out.resolved_engine == "dual" ||
        out.resolved_engine == "ipm" || out.resolved_engine == "pdlp";
    if ((linear_engine && model.has_quadratic_objective) ||
        (nonlinear && out.resolved_engine != "sqp" && out.resolved_engine != "outer_approx")) {
        result.status = lp::reference::SolveStatus::unsupported;
        result.message = "selected engine cannot represent this objective or constraints"; return;
    }

    if (out.resolved_engine == "sqp" || out.resolved_engine == "outer_approx")
        run_nonlinear(model, options, out, result);
    else if (out.resolved_engine == "parallel") run_parallel(model, options, out, result);
    else if (out.resolved_engine == "pdlp") run_pdlp(model, options, out, result);
    else if (out.resolved_engine == "qp") run_qp(model, options, out, result);
    else if (out.resolved_engine == "milp" || out.resolved_engine == "miqp")
        run_milp(model, options, out, result);
    else run_lp(model, options, out, result);
    if (out.original_primal.size() == model.matrix.column_count) {
        out.row_activities = model.matrix.multiply(out.original_primal);
        for (std::size_t i = 0; i < out.row_activities.size(); ++i) {
            out.row_lower_slacks.push_back(model.row_lower[i].is_finite()
                ? out.row_activities[i] - model.row_lower[i].value : std::numeric_limits<double>::quiet_NaN());
            out.row_upper_slacks.push_back(model.row_upper[i].is_finite()
                ? model.row_upper[i].value - out.row_activities[i] : std::numeric_limits<double>::quiet_NaN());
        }
    }
}

} // namespace markov_cero::api::detail
