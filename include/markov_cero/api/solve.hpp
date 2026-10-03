#pragma once

#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/model/classifier.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/verify/primal_verifier.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include "markov_cero/verify/mip_proof.hpp"
#include "markov_cero/verify/oa_proof.hpp"
#include <string>
#include <vector>

namespace markov_cero::lp::dual { class Session; }
namespace markov_cero::qp { class AdmmQpSolver; }

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
    bool enable_pdlp_crossover{true};
    bool enable_mip_proof{true};
    double mip_proof_time_limit_seconds{5.0};
    std::size_t mip_proof_max_nodes{10000};
    std::size_t mip_proof_max_witness_values{4000000};
    std::string backend = "cpu";
    // Optional hard cap applied before an MPS/LP file is tokenized. Unset
    // preserves the format-specific parser defaults.
    std::optional<std::size_t> maximum_input_bytes;
    lp::reference::Options lp_options;
    milp::Options milp_options;
    // Optional caller-owned repeated-solve state. Reuse each session sequentially;
    // concurrent solves should each receive their own session instance.
    lp::dual::Session* repeated_lp_session{nullptr};
    qp::AdmmQpSolver* repeated_qp_session{nullptr};
    // W02/IR-20: solve-wide wall clock measured from API entry, covering
    // parsing, engine work, verification and proof stages. Must be > 0 and
    // <= 1e8 seconds when set; unset keeps the engine-local limits only.
    std::optional<double> total_time_limit_seconds;
    // W02/IR-21: solve-wide budget for solver-owned bytes admitted at the
    // instrumented charge points. Must be > 0 when set; unset disables the
    // budget (charges are still accounted but never refused).
    std::optional<std::size_t> memory_limit_bytes;
    // RES-01: solve-scoped budget for bytes admitted through the gpu buffer
    // layer during a `backend == "gpu"` solve. Must be > 0 when set; unset
    // keeps the accounting and disables the refusal, mirroring
    // `memory_limit_bytes` (contract docs/contracts/resource-limits.md §4).
    std::optional<std::size_t> device_memory_limit_bytes;
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
    // QP-01 (convex-qp.md §5): what actually executed for this solve, as
    // opposed to `backend` (the request) and `recommended_backend` (the
    // threshold recommendation): "cpu", "cuda", or "cpu_fallback" (GPU was
    // requested but the activation gate failed). Set by engines that own a
    // GPU path (QP, PDLP); "cpu" everywhere else.
    std::string backend_actually_used = "cpu";
    NumericalDiagnostic diagnostic;

    // D-15: set when an engine terminated for a reason that is not a hard
    // failure but deserves explicit reporting (e.g. PDLP stagnation that
    // triggered a failed dual-simplex crossover). Empty otherwise.
    std::string convergence_note;

    // W02/IR-20 + resource contract (docs/contracts/resource-limits.md): the
    // first recorded resource stop for this solve — a cooperative context
    // stop (deadline, cancellation, memory budget, quota) or an API-boundary
    // stop (input_limit, work_limit, allocation_failure). Non-empty on every
    // `resource_limit` result (unattributable engine-internal stops report
    // `unspecified_resource_limit`); empty when nothing stopped the solve; it
    // never appears next to an unverified optimal/infeasible/unbounded status.
    std::string stop_reason;

    // IR-21 peak diagnostic (resource contract section 2): high-water mark of
    // solver-owned bytes admitted by the solve-wide MemoryBudget; 0 when no
    // charge ran. Instrumented charges only — never an RSS measurement
    // (contract section 4).
    std::size_t memory_charged_peak_bytes = 0;

    // RES-01 peak diagnostic (resource contract section 4): high-water mark
    // of device-buffer bytes admitted by the solve-scoped DeviceBudget; 0
    // when no device budget was installed (every non-gpu solve). The gpu
    // buffer layer only — never a cudaMemGetInfo reading.
    std::size_t device_memory_charged_peak_bytes = 0;

    // D-16: how often the ADMM penalty rho changed (each change triggers one
    // KKT re-factorization). 0 for every non-QP engine.
    std::size_t admm_rho_updates = 0;

    std::size_t model_rows = 0;
    std::size_t model_cols = 0;
    std::size_t model_nnz = 0;
    // W01/D16: stable identity of the validated model captured at the API
    // boundary (mix of structural and numeric content hashes; 0 only when no
    // model reached the boundary). Binds results and later proof artifacts to
    // the exact model that produced them.
    std::uint64_t model_fingerprint = 0;

    std::vector<double> primal;
    double objective = 0.0;
    std::vector<double> original_primal;
    double original_objective = 0.0;

    // Global verification is distinct from a checked incumbent or local KKT point.
    std::string certificate_type{"none"};
    // Contract v1 (docs/contracts/numerical-policy.md section 2): the strongest
    // check that actually passed, derived once in api::detail::finalize from
    // status, the verification flags, certificate_type and guarantee_tier.
    // Engines never set it. Values: tree_replayed, optimality_witness_checked,
    // local_kkt_checked, original_primal_checked, unverified. Additive: no
    // existing field changes meaning.
    std::string assurance{"unverified"};
    // Proof assurance is separate from verified: an accepted gap certificate
    // bounds suboptimality but does not establish exact optimality.
    std::string guarantee_tier{"unverified"};
    std::string proof_status{"not_requested"};
    std::string proof_budget_kind{"none"};
    bool proof_budget_exhausted{false};
    std::size_t proof_nodes_used{0};
    std::size_t proof_checked_nodes{0};
    std::size_t proof_witness_values_used{0};
    std::size_t proof_checked_witness_values{0};
    double proof_budget_time_ms{0.0};
    std::uint32_t proof_format_version{0};
    std::string proof_model_fingerprint;
    // Independent MIP certificate construction and replay cost, excluding the
    // primary solve. Zero when the proof path was not requested or not entered.
    double mip_proof_build_ms{0.0};
    double mip_proof_verify_ms{0.0};
    std::shared_ptr<const verify::MipProof> mip_proof;
    // MINLP-02 (minlp-proof-replay.md §6.2): independent OA proof record and
    // its construction/replay cost, alongside the MIP proof family.
    double oa_proof_build_ms{0.0};
    double oa_proof_verify_ms{0.0};
    std::shared_ptr<const verify::OaProof> oa_proof;
    std::string proof_message;
    std::vector<std::string> variable_names;
    std::vector<std::string> row_names;
    std::vector<double> row_activities;
    std::vector<double> row_lower_slacks;
    std::vector<double> row_upper_slacks;
    std::vector<double> row_duals;
    std::vector<double> reduced_costs;
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
    double best_bound = std::numeric_limits<double>::quiet_NaN();
    double relative_gap = std::numeric_limits<double>::infinity();
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
