#include "markov_cero/model/classifier.hpp"

#include <string>

namespace markov_cero::model {

const char* to_string(ProblemClass problem_class) noexcept {
    switch (problem_class) {
    case ProblemClass::lp: return "LP";
    case ProblemClass::milp: return "MILP";
    case ProblemClass::qp: return "QP";
    case ProblemClass::miqp: return "MIQP";
    case ProblemClass::nlp: return "NLP";
    case ProblemClass::minlp: return "MINLP";
    }
    return "LP";
}

ClassificationStats classify_stats(const Model& model) {
    ClassificationStats stats;
    stats.nonzeros = model.matrix.value.size();
    stats.has_quadratic_objective = model.has_quadratic_objective;
    stats.quadratic_nonzeros = model.has_quadratic_objective
                                   ? model.quadratic_matrix.value.size()
                                   : 0;
    for (const auto type : model.variable_type) {
        switch (type) {
        case VariableType::integer:
            ++stats.integer_variables;
            break;
        case VariableType::binary:
            ++stats.binary_variables;
            break;
        case VariableType::continuous:
            ++stats.continuous_variables;
            break;
        }
    }
    return stats;
}

ClassificationResult classify_model(const Model& model, const ClassificationInputs& inputs) {
    ClassificationResult out;
    ClassificationStats stats = classify_stats(model);
    const bool discrete = stats.integer_variables > 0 || stats.binary_variables > 0;

    // LOCKED decision tree (plan W6 section 6.1). Exact order, no other logic.
    if (inputs.has_nlp_callbacks) {
        out.problem_class = discrete ? ProblemClass::minlp : ProblemClass::nlp;
        out.reason = "nlp_callbacks";
    } else if (inputs.has_nlobj_section) {
        out.problem_class = discrete ? ProblemClass::minlp : ProblemClass::nlp;
        out.reason = "nlobj_section";
    } else if (model.has_quadratic_objective) {
        out.problem_class = discrete ? ProblemClass::miqp : ProblemClass::qp;
        out.reason = "quadratic_objective";
    } else if (discrete) {
        out.problem_class = ProblemClass::milp;
        out.reason = "integer_variables";
    } else {
        out.problem_class = ProblemClass::lp;
        out.reason = "continuous_linear";
    }

    out.default_engine = select_engine(out.problem_class, stats, /*backend_gpu_requested=*/false);
    return out;
}

std::string select_engine(ProblemClass problem_class, const ClassificationStats& stats,
                          bool /*backend_gpu_requested*/) {
    // Note: the backend recommendation (GPU vs CPU) is decided by the caller
    // (api.cpp) using the engine + threshold gates; the classifier only
    // selects the engine. The parameter is kept in the signature for API
    // symmetry with the locked rule table.
    switch (problem_class) {
    case ProblemClass::lp:
        // LOCKED threshold: NNZ > 50,000 dispatches LP to first-order PDLP.
        if (stats.nonzeros > EngineThresholds::kLpPdlpNnzThreshold) {
            return "pdlp";
        }
        return "primal";
    case ProblemClass::milp:
        // The parallel engine is chosen by the caller when --threads > 1;
        // the classifier reports the single-thread default.
        return "milp";
    case ProblemClass::qp:
        // LOCKED threshold: NNZ of P > 100,000 with an explicit GPU request
        // enables the GPU ADMM path; CPU ADMM otherwise. The engine label is
        // "qp" in both cases; the backend decision is recorded separately.
        return "qp";
    case ProblemClass::miqp:
        return "miqp";
    case ProblemClass::nlp:
        return "sqp";
    case ProblemClass::minlp:
        return "outer_approx";
    }
    return "primal";
}

} // namespace markov_cero::model
