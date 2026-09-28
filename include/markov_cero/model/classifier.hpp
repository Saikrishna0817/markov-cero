#pragma once

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <string>

namespace markov_cero::model {

// W6 problem classification (implementation plan, locked decision tree).
// Determines the problem class from structural properties of the parsed model:
//   - NLP callbacks (Path A) or an NLOBJ MPS section (Path B)  -> NLP / MINLP
//   - quadratic objective (QUADOBJ/QMATRIX sections)          -> QP  / MIQP
//   - integer/binary variables                                -> MILP
//   - otherwise                                               -> LP
//
// The tree is LOCKED by the implementation plan (W6, section 6.1): no other
// classification logic is permitted. Engine thresholds are LOCKED constants;
// changing them requires new benchmark evidence and owner approval.

enum class ProblemClass { lp, milp, qp, miqp, nlp, minlp };

[[nodiscard]] const char* to_string(ProblemClass problem_class) noexcept;

struct ClassificationInputs {
    // True when an NlpModel with non-null callbacks accompanies the model
    // (W1 Path A, programmatic input). File parsing leaves this false.
    bool has_nlp_callbacks{false};
    // True when the MPS file carried an NLOBJ section (W1 Path B, file input).
    bool has_nlobj_section{false};
};

struct ClassificationResult {
    ProblemClass problem_class{ProblemClass::lp};
    // Locked-rule engine auto-selection for the classified problem.
    std::string default_engine;
    // Backend recommendation honoring the locked NNZ thresholds. "gpu" is only
    // ever produced when the caller explicitly requested the GPU backend AND
    // the instance is above the size threshold; anything else falls back to
    // "cpu" (plan W4: never crash on a GPU request — fall back and log).
    std::string recommended_backend{"cpu"};
    // Human-readable reason recorded in solver telemetry / JSON output.
    std::string reason;
};

struct ClassificationStats {
    std::size_t integer_variables{0};
    std::size_t binary_variables{0};
    std::size_t continuous_variables{0};
    std::size_t nonzeros{0};
    std::size_t quadratic_nonzeros{0};
    bool has_quadratic_objective{false};
};

[[nodiscard]] ClassificationStats classify_stats(const Model& model);

// The locked decision tree. Both `model` and `inputs` feed it; the tree order is
// NLP callbacks -> NLOBJ section -> quadratic objective -> integer variables -> LP.
[[nodiscard]] ClassificationResult classify_model(const Model& model,
                                                 const ClassificationInputs& inputs);

// Locked engine-selection table (plan W6). `backend_gpu_requested` reflects an
// explicit `--backend gpu` request; thresholds below are LOCKED constants.
struct EngineThresholds {
    // LP auto-dispatch switches from primal simplex to first-order PDLP above
    // this structural nonzero count.
    static constexpr std::size_t kLpPdlpNnzThreshold = 50000;
    // LP PDLP dispatch is allowed to use the GPU backend above this count
    // (only when the caller explicitly requested the GPU backend).
    static constexpr std::size_t kPdlpGpuNnzThreshold = 500000;
    // QP dispatch may use the GPU ADMM kernel above this quadratic-matrix
    // nonzero count (only when the caller explicitly requested the GPU backend).
    static constexpr std::size_t kQpGpuNnzThreshold = 100000;
};

[[nodiscard]] std::string select_engine(ProblemClass problem_class,
                                       const ClassificationStats& stats,
                                       bool backend_gpu_requested);

} // namespace markov_cero::model
