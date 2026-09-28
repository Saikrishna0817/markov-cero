#pragma once

#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/model/classifier.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/verify/primal_verifier.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace markov_cero::api {

struct SolveOptions {
    std::string engine = "auto";
    std::size_t num_threads = 4;
    // True when the thread count was explicitly requested (CLI --threads or a
    // programmatic caller). W6 auto-dispatch upgrades milp -> parallel only on
    // an explicit request; the default value alone must not change dispatch.
    bool threads_explicit = false;
    std::string warm_start_path;
    std::string save_basis_path;
    bool enable_presolve = true;
    bool enable_scale = true;
    std::size_t max_presolve_passes = 5;
    std::size_t ruiz_iterations = 10;
    double pdlp_tolerance = 1e-4;
    std::string backend = "cpu";
    lp::reference::Options lp_options;
    milp::Options milp_options;
};

struct NumericalDiagnostic {
    double primal_residual{0.0};
    double dual_residual{0.0};
    // C-3: |c^T x - b^T y| evaluated at the reported solution. 0.0 when the
    // engine reports no dual vector (MILP incumbents, NLP/MINLP).
    double complementarity_gap{0.0};
    // Independently recomputed NLP KKT components; zero for non-NLP engines.
    double nlp_stationarity_residual{0.0};
    double nlp_inequality_violation{0.0};
    double nlp_equality_violation{0.0};
    double nlp_worst_dual_sign{0.0};
    double nlp_complementarity_residual{0.0};
    // max|Uii|/min|Uii| pivot-ratio condition proxy of the final factorization
    // (basis LU for simplex, A D A^T for IPM, KKT LDL^T diagonal for QP).
    // Always >= 1.0 when computed; 0.0 means the engine performed no
    // factorization (matrix-free PDLP, SQP) so no estimate exists.
    double condition_estimate{0.0};
    std::string failure_site;
    std::string suggested_recovery;
};

struct SolveResult {
    lp::reference::SolveStatus status{lp::reference::SolveStatus::numerical_failure};
    std::string message;
    std::string resolved_engine = "auto";
    std::string problem_class = "LP";
    std::string classification_reason;
    std::string recommended_backend = "cpu";
    NumericalDiagnostic diagnostic;

    // D-15: set when an engine terminated for a reason that is not a hard
    // failure but deserves explicit reporting (e.g. PDLP stagnation that
    // triggered a failed dual-simplex crossover). Empty otherwise.
    std::string convergence_note;

    // D-16: how often the ADMM penalty rho changed (each change triggers one
    // KKT re-factorization). 0 for every non-QP engine.
    std::size_t admm_rho_updates = 0;

    std::size_t model_rows = 0;
    std::size_t model_cols = 0;
    std::size_t model_nnz = 0;

    std::vector<double> primal;
    double objective = 0.0;
    std::vector<double> original_primal;
    double original_objective = 0.0;

    bool verified = false;
    bool canonical_verified = false;
    bool original_verified = false;
    std::string original_message;
    verify::PrimalVerificationReport primal_report;
    verify::ReferenceVerification canonical_report;

    bool used_warm_start = false;
    bool used_cold_fallback = false;

    double runtime_ms = 0.0;
    std::size_t nodes_explored = 0;
    std::size_t lp_iterations = 0;
    double best_bound = 0.0;
    double relative_gap = 0.0;
    std::size_t cuts_generated = 0;
    std::size_t heuristics_found = 0;
    bool ml_requested = false;
    bool ml_model_loaded = false;
    std::size_t ml_scoring_calls = 0;
    std::size_t ml_candidates_scored = 0;
    std::size_t ml_fallback_nodes = 0;
    std::size_t ml_maximum_candidate_count = 0;
    std::string ml_fallback_reason;
    std::size_t phase_one_iterations = 0;
    std::size_t phase_two_iterations = 0;

    double pdlp_primal_infeasibility = 0.0;
    double pdlp_dual_infeasibility = 0.0;
    double pdlp_duality_gap = 0.0;
    double pdlp_h2d_ms = 0.0;
    double pdlp_kernel_ms = 0.0;
    double pdlp_d2h_ms = 0.0;
    double pdlp_total_ms = 0.0;

    bool input_open_failed = false;
    std::string error;
};

[[nodiscard]] SolveResult solve_file(const std::string& path, const SolveOptions& options = {});
[[nodiscard]] SolveResult solve_model(const model::Model& model,
                                      const SolveOptions& options = {});

} // namespace markov_cero::api
