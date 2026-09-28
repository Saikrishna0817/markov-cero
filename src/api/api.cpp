#include "markov_cero/api/solve.hpp"

#include "markov_cero/io/lp_parser.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/minlp/minlp_solver.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/interior/ipm.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/milp/parallel_tree_search.hpp"
#include "markov_cero/presolve/presolve.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/model.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/scale/ruiz_scaling.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

namespace markov_cero::api {
namespace {

using Clock = std::chrono::steady_clock;

SolveOptions with_api_deadline(SolveOptions options, Clock::time_point started) {
    if (!options.lp_options.deadline &&
        std::isfinite(options.lp_options.time_limit_seconds) &&
        options.lp_options.time_limit_seconds > 0.0) {
        options.lp_options.deadline = started +
            std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(options.lp_options.time_limit_seconds));
    }
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

void run_engine(const model::Model& model, const SolveOptions& options, SolveResult& out,
                lp::reference::Result& result) {
    const auto deadline_expired = [&] {
        return options.lp_options.deadline && Clock::now() >= *options.lp_options.deadline;
    };
    const auto stop_after_deadline = [&](const char* phase) {
        if (!deadline_expired()) return false;
        result.status = lp::reference::SolveStatus::resource_limit;
        result.message = std::string("wall-clock deadline reached after ") + phase;
        out.status = result.status;
        out.message = result.message;
        out.diagnostic.failure_site = "api_wall_clock_deadline";
        out.diagnostic.suggested_recovery = "increase_time_limit_or_reduce_preprocessing_cost";
        return true;
    };
    if (stop_after_deadline("input parsing or before engine dispatch")) return;
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

    std::optional<lp::dual::BasisState> basis_to_save;

    // W1: NLP engines. Path B file models arrive as parsed NLOBJ terms; Path A
    // callback companions ride on model.nlp_callbacks; make_nlp_model composes
    // whichever are present into the SQP/outer-approx view.
    if (out.resolved_engine == "sqp" || out.resolved_engine == "outer_approx") {
        io::NlobjBridge bridge{model};
        const auto nlp_model = bridge.build();
        if (stop_after_deadline("NLP model construction")) return;
        // Deterministic start: midpoint of the variable bounds (0 where free).
        std::vector<double> x0(nlp_model.n_vars, 0.0);
        for (std::size_t j = 0; j < nlp_model.n_vars; ++j) {
            const double lo = nlp_model.bound_lower(j);
            const double hi = nlp_model.bound_upper(j);
            if (std::isfinite(lo) && std::isfinite(hi)) {
                x0[j] = 0.5 * (lo + hi);
            }
        }
        if (out.resolved_engine == "sqp") {
            nlp::SqpOptions sqp_opts;
            sqp_opts.deadline = options.lp_options.deadline;
            const auto sol = nlp::solve_sqp(nlp_model, x0, sqp_opts);
            out.lp_iterations = sol.iterations;
            out.diagnostic.primal_residual = sol.constraint_violation;
            out.diagnostic.dual_residual = sol.kkt_residual;
            // SQP factorizes nothing per solve; 0.0 = not estimated.
            out.diagnostic.condition_estimate = 0.0;
            result.status = sol.status;
            result.message = sol.message;
            if (sol.status == lp::reference::SolveStatus::optimal) {
                // C4: independent KKT verification before reporting optimal.
                const auto rep = nlp::verify_nlp_solution(nlp_model, sol, 1e-6);
                out.diagnostic.nlp_stationarity_residual = rep.stationarity_residual;
                out.diagnostic.nlp_inequality_violation = rep.inequality_violation;
                out.diagnostic.nlp_equality_violation = rep.equality_violation;
                out.diagnostic.nlp_worst_dual_sign = rep.worst_dual_sign;
                out.diagnostic.nlp_complementarity_residual = rep.complementarity_residual;
                if (rep.accepted) {
                    result.primal = sol.x;
                    result.objective = sol.objective;
                    out.original_primal = sol.x;
                    out.original_objective = sol.objective;
                    out.original_verified = true;
                    out.canonical_verified = true;
                    out.original_message = rep.message;
                } else {
                    result.status = lp::reference::SolveStatus::numerical_failure;
                    result.message = "nlp KKT verification failed: " + rep.message;
                    out.original_message = rep.message;
                    out.diagnostic.failure_site = "nlp_kkt_verification";
                    out.diagnostic.suggested_recovery = "relax_tolerance_or_improve_x0";
                }
            } else {
                out.canonical_verified = true;
                out.original_message = "original primal not applicable";
                out.diagnostic.failure_site = "sqp_solve";
                out.diagnostic.suggested_recovery = "improve_x0_or_relax_kkt_tolerance";
            }
        } else {
            minlp::MinlpProblem problem;
            problem.nlp = nlp_model;
            problem.source_model = model;
            for (std::size_t j = 0; j < nlp_model.n_vars; ++j) {
                if (model.variable_type[j] != model::VariableType::continuous) {
                    problem.integer_indices.push_back(j);
                }
            }
            minlp::MinlpOptions minlp_opts;
            minlp_opts.deadline = options.lp_options.deadline;
            minlp_opts.sqp_options.deadline = options.lp_options.deadline;
            const auto sol = minlp::solve_minlp(problem, x0, minlp_opts);
            out.lp_iterations = sol.iterations;
            out.cuts_generated = sol.cuts_added;
            const double sense_sign = model.objective_sense == model::ObjectiveSense::maximize
                                          ? -1.0
                                          : 1.0;
            out.best_bound = sense_sign * sol.best_bound;
            out.relative_gap = sol.relative_gap;
            result.status = sol.status;
            result.message = sol.message;
            if (sol.integer_feasible && sol.x.size() == nlp_model.n_vars) {
                const auto feasibility = nlp::verify_nlp_feasibility(
                    nlp_model, sol.x, minlp_opts.feasibility_tolerance);
                bool integral = true;
                for (std::size_t j = 0; j < model.variable_type.size(); ++j) {
                    if (model.variable_type[j] != model::VariableType::continuous &&
                        std::abs(sol.x[j] - std::round(sol.x[j])) >
                            minlp_opts.feasibility_tolerance) {
                        integral = false;
                        break;
                    }
                }
                const double normalized_objective = nlp_model.eval_objective(sol.x);
                const bool objective_consistent =
                    std::isfinite(normalized_objective) && std::isfinite(sol.objective) &&
                    std::abs(normalized_objective - sol.objective) <=
                        minlp_opts.feasibility_tolerance *
                            std::max(1.0, std::abs(normalized_objective));
                out.diagnostic.primal_residual = feasibility.maximum_violation;
                result.primal = sol.x;
                result.objective = sense_sign * normalized_objective;
                out.original_primal = sol.x;
                out.original_objective = result.objective;
                out.original_verified = feasibility.feasible && integral && objective_consistent;
                out.canonical_verified =
                    sol.status == lp::reference::SolveStatus::optimal &&
                    std::isfinite(sol.best_bound) && std::isfinite(sol.relative_gap) &&
                    sol.relative_gap <= minlp_opts.gap_tolerance;
                if (!out.original_verified) {
                    result.status = lp::reference::SolveStatus::numerical_failure;
                    result.message = "minlp: independent incumbent verification failed (" +
                                     feasibility.message + (integral ? "" : "; non-integral") +
                                     (objective_consistent ? "" : "; objective mismatch") + ")";
                    out.original_message = result.message;
                    out.diagnostic.failure_site = "minlp_incumbent_verification";
                    out.diagnostic.suggested_recovery = "inspect_minlp_subproblem_feasibility";
                } else if (sol.status == lp::reference::SolveStatus::optimal &&
                           !out.canonical_verified) {
                    result.status = lp::reference::SolveStatus::numerical_failure;
                    result.message = "minlp: solver reported optimal without a finite, "
                                     "within-tolerance global bound";
                    out.original_message = result.message;
                    out.diagnostic.failure_site = "minlp_global_bound_verification";
                    out.diagnostic.suggested_recovery = "inspect_oa_master_bound_and_gap";
                } else {
                    out.original_message = out.canonical_verified
                                               ? "MINLP incumbent feasibility verified; "
                                                 "convex OA bound and gap establish numerical global optimality"
                                               : "MINLP incumbent feasibility verified; "
                                                 "global optimality not established";
                }
            } else {
                out.original_verified = false;
                out.canonical_verified = false;
                out.original_message = "no feasible MINLP incumbent returned; no primal verification applies";
                if (sol.status == lp::reference::SolveStatus::optimal) {
                    result.status = lp::reference::SolveStatus::numerical_failure;
                    result.message = "minlp: optimal status without a feasible incumbent";
                    out.diagnostic.failure_site = "minlp_incumbent_missing";
                    out.diagnostic.suggested_recovery = "inspect_oa_incumbent_updates";
                } else {
                    out.diagnostic.failure_site = "minlp_solve";
                    out.diagnostic.suggested_recovery = "inspect_cut_accumulation_and_x0";
                }
            }
        }
        out.nodes_explored = 1;
        return;
    }

    if (out.resolved_engine == "parallel") {
        milp::ParallelOptions par_opts;
        par_opts.num_threads = options.num_threads;
        par_opts.time_limit_seconds = options.milp_options.time_limit_seconds;
        par_opts.deadline = options.lp_options.deadline;
        par_opts.max_nodes = options.milp_options.max_nodes;
        par_opts.enable_cuts = options.milp_options.enable_cuts;
        par_opts.enable_mir_cuts = options.milp_options.enable_mir_cuts;
        par_opts.enable_heuristics = options.milp_options.enable_heuristics;
        par_opts.enable_strong_branching = options.milp_options.enable_strong_branching;
        par_opts.branching_strategy = options.milp_options.branching_strategy;
        const auto par_res = milp::solve_parallel(model, par_opts);
        result.status = par_res.status;
        result.message = par_res.message;
        out.nodes_explored = par_res.nodes_explored;
        out.lp_iterations = par_res.lp_iterations;
        out.best_bound = par_res.best_bound;
        out.relative_gap = par_res.relative_gap;
        out.cuts_generated = par_res.cuts_generated;
        out.heuristics_found = par_res.heuristics_found;
        out.diagnostic.condition_estimate = par_res.condition_estimate;

        if (result.status == lp::reference::SolveStatus::optimal) {
            out.original_primal = par_res.primal;
            out.original_objective = par_res.objective;
            result.primal = par_res.primal;
            result.objective = par_res.objective;

            verify::Candidate candidate{out.original_primal, out.original_objective};
            out.primal_report = verify::verify_primal(model, candidate);
            out.original_verified = out.primal_report.passed;
            out.canonical_verified = true;
            const auto viol_desc = format_violation(out.primal_report);
            out.original_message = out.original_verified
                                       ? "original primal verified"
                                       : ("original primal rejected: " + viol_desc);
            if (!out.original_verified) {
                result.status = lp::reference::SolveStatus::numerical_failure;
                result.message = "original-model verification failed: " + viol_desc;
            }
        } else if (result.status == lp::reference::SolveStatus::infeasible ||
                   result.status == lp::reference::SolveStatus::unbounded) {
            out.canonical_verified = true;
            out.original_message = "original primal not applicable";
        } else {
            out.original_message = "original primal not applicable";
        }
    } else if (out.resolved_engine == "pdlp") {
        lp::first_order::PdlpOptions pdlp_opts;
        pdlp_opts.backend = (options.backend == "gpu") ? lp::first_order::Backend::gpu
                                                       : lp::first_order::Backend::cpu;
        pdlp_opts.max_iterations = (options.lp_options.iteration_limit != 10000 &&
                                    options.lp_options.iteration_limit > 0)
                                       ? options.lp_options.iteration_limit
                                       : 100000;
        pdlp_opts.set_tolerance(options.pdlp_tolerance);
        pdlp_opts.enable_crossover = true;
        pdlp_opts.deadline = options.lp_options.deadline;
        const auto pdlp_res = lp::first_order::solve_pdlp(model, pdlp_opts);
        out.lp_iterations = pdlp_res.iterations;
        out.pdlp_primal_infeasibility = pdlp_res.primal_infeasibility;
        out.pdlp_dual_infeasibility = pdlp_res.dual_infeasibility;
        out.pdlp_duality_gap = pdlp_res.duality_gap;
        out.pdlp_h2d_ms = pdlp_res.h2d_ms;
        out.pdlp_kernel_ms = pdlp_res.kernel_ms;
        out.pdlp_d2h_ms = pdlp_res.d2h_ms;
        out.pdlp_total_ms = pdlp_res.total_ms;
        out.diagnostic.primal_residual = pdlp_res.primal_infeasibility;
        out.diagnostic.dual_residual = pdlp_res.dual_infeasibility;
        // D-15: explicit stagnation/crossover note surfaced in JSON output.
        out.convergence_note = pdlp_res.convergence_note;
        // C-3: |pobj - dobj| of the reported PDLP iterate (the engine's dual
        // objective carries the row- and bound-complementary terms).
        out.diagnostic.complementarity_gap =
            std::abs(pdlp_res.objective - pdlp_res.dual_objective);
        // Matrix-free PDLP has no factorization; only the crossover basis
        // (when one was certified) contributes an estimate.
        out.diagnostic.condition_estimate = pdlp_res.condition_estimate;
        if (pdlp_res.status == lp::first_order::PdlpStatus::optimal) {
            out.diagnostic.failure_site = "none";
            out.diagnostic.suggested_recovery = "none";
            result.status = lp::reference::SolveStatus::optimal;
            result.primal = pdlp_res.primal;
            result.dual = pdlp_res.dual;
            result.objective = pdlp_res.objective;
            result.message = pdlp_res.message;
            out.original_primal = pdlp_res.primal;
            out.original_objective = pdlp_res.objective;

            verify::Candidate candidate{out.original_primal, out.original_objective};
            const verify::Tolerance pdlp_tol{options.pdlp_tolerance, options.pdlp_tolerance};
            out.primal_report = verify::verify_primal(model, candidate, pdlp_tol, pdlp_tol,
                                                      options.pdlp_tolerance);
            out.original_verified = out.primal_report.passed;
            out.canonical_verified = true;
            const auto viol_desc = format_violation(out.primal_report);
            out.original_message = out.original_verified
                                       ? "original primal verified"
                                       : ("original primal rejected: " + viol_desc);
            if (!out.original_verified) {
                result.status = lp::reference::SolveStatus::numerical_failure;
                result.message = "original-model verification failed: " + viol_desc;
                out.diagnostic.failure_site = "pdlp_kkt_verification";
                out.diagnostic.suggested_recovery = "tighten_pdlp_tolerance_or_crossover";
            }
        } else if (pdlp_res.status == lp::first_order::PdlpStatus::iteration_limit) {
            result.status = lp::reference::SolveStatus::iteration_limit;
            result.message = pdlp_res.message;
            out.diagnostic.failure_site = "pdlp_iteration_limit";
            out.diagnostic.suggested_recovery = "crossover_to_dual_simplex_or_increase_iterations";
        } else if (pdlp_res.status == lp::first_order::PdlpStatus::resource_limit) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = pdlp_res.message;
            out.diagnostic.failure_site = "pdlp_wall_clock_deadline";
            out.diagnostic.suggested_recovery = "increase_time_limit_or_use_a_warm_start";
        } else {
            result.status = lp::reference::SolveStatus::numerical_failure;
            result.message = pdlp_res.message;
            out.diagnostic.failure_site = "pdlp_first_order_solver";
            out.diagnostic.suggested_recovery = "crossover_to_dual_simplex_or_tighten_step_sizes";
        }
        out.nodes_explored = 1;
        out.best_bound = out.original_objective;
        out.relative_gap = 0.0;
    } else if (out.resolved_engine == "qp") {
        const auto qp_model = qp::make_quadratic_model(model);
        if (stop_after_deadline("QP model construction")) return;
        qp::QpOptions qopts;
        qopts.absolute_tolerance = 1e-6;
        qopts.relative_tolerance = 1e-6;
        qopts.max_iterations = (options.lp_options.iteration_limit > 0)
                                   ? options.lp_options.iteration_limit
                                   : 10000;
        qopts.deadline = options.lp_options.deadline;
        qopts.time_limit_seconds = options.lp_options.time_limit_seconds;
        // W3/D-08: GPU ADMM path requires explicit --backend gpu; the threshold
        // and device checks are enforced inside the solver (silent CPU fallback).
        const bool gpu_requested = options.backend == "gpu";
        if (gpu_requested) {
            out.recommended_backend = "gpu";
        }
        const auto qpres = qp::solve_qp(qp_model, qopts, gpu_requested);
        out.lp_iterations = qpres.iterations;
        out.nodes_explored = 1;
        out.best_bound = qpres.objective_value;
        out.relative_gap = 0.0;
        // D-16: rho changes (each one re-factorized the KKT matrix).
        out.admm_rho_updates = qpres.refactorization_count;
        out.diagnostic.primal_residual = qpres.primal_residual;
        out.diagnostic.dual_residual = qpres.dual_residual;
        out.diagnostic.condition_estimate = qpres.condition_estimate;
        if (qpres.status == qp::QpStatus::optimal) {
            out.diagnostic.failure_site = "none";
            out.diagnostic.suggested_recovery = "none";
            result.status = lp::reference::SolveStatus::optimal;
            result.primal = qpres.x;
            result.objective = qpres.objective_value;
            result.message = "QP solved to optimality";
            out.original_primal = qpres.x;
            out.original_objective = qpres.objective_value;
            const auto rep = qp::verify_qp_solution(qp_model, qpres, 1e-4);
            out.original_verified = rep.passed;
            out.canonical_verified = true;
            out.original_message = rep.passed ? "QP KKT certificate verified"
                                              : ("QP verification failed: " + rep.failure_reason);
            if (!rep.passed) {
                result.status = lp::reference::SolveStatus::numerical_failure;
                result.message = out.original_message;
                out.diagnostic.failure_site = "qp_kkt_verification";
                out.diagnostic.suggested_recovery = "tighten_qp_tolerance_or_regularize";
            }
        } else if (qpres.status == qp::QpStatus::primal_infeasible) {
            result.status = lp::reference::SolveStatus::infeasible;
            result.message = "QP primal infeasible";
            out.canonical_verified = true;
            out.original_message = "original primal not applicable";
            out.diagnostic.failure_site = "qp_primal_infeasibility_certificate";
            out.diagnostic.suggested_recovery = "relax_incompatible_row_or_variable_bounds";
        } else if (qpres.status == qp::QpStatus::dual_infeasible) {
            result.status = lp::reference::SolveStatus::unbounded;
            result.message = "QP dual infeasible (unbounded)";
            out.canonical_verified = true;
            out.original_message = "original primal not applicable";
            out.diagnostic.failure_site = "qp_dual_infeasibility_certificate";
            out.diagnostic.suggested_recovery = "add_missing_variable_bounds_or_regularize_cost";
        } else if (qpres.status == qp::QpStatus::non_convex) {
            result.status = lp::reference::SolveStatus::invalid_model;
            result.message = "Non-convex QP objective is not supported: " + qpres.message;
            out.original_message = result.message;
            out.diagnostic.failure_site = "qp_convexity_check";
            out.diagnostic.suggested_recovery = "ensure_quadratic_objective_matrix_P_is_positive_semidefinite";
        } else if (qpres.status == qp::QpStatus::unsupported) {
            result.status = lp::reference::SolveStatus::unsupported;
            result.message = "QP unsupported: " + qpres.message;
            out.original_message = result.message;
            const bool kkt_fill_limit = qpres.message == "sparse QP KKT factor exceeds fill limit";
            out.diagnostic.failure_site = kkt_fill_limit ? "qp_kkt_factor_fill_limit"
                                                         : "qp_convexity_check_indeterminate";
            out.diagnostic.suggested_recovery = kkt_fill_limit
                ? "use_a_sparser_formulation_or_an_alternative_qp_method"
                : "rescale_or_provide_a_numerically_certifiable_convex_model";
        } else if (qpres.status == qp::QpStatus::iteration_limit) {
            result.status = lp::reference::SolveStatus::iteration_limit;
            result.message = std::string("QP solve failed: ") + qp::to_string(qpres.status);
            out.original_message = result.message;
            out.diagnostic.failure_site = "qp_admm_iteration_limit";
            out.diagnostic.suggested_recovery = "increase_iteration_limit_or_tune_rho_init";
        } else if (qpres.status == qp::QpStatus::time_limit) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = "QP wall-clock deadline reached";
            out.original_message = result.message;
            out.diagnostic.failure_site = "qp_wall_clock_deadline";
            out.diagnostic.suggested_recovery = "increase_time_limit_or_use_a_warm_start";
        } else {
            result.status = lp::reference::SolveStatus::numerical_failure;
            result.message = std::string("QP solve failed: ") + qp::to_string(qpres.status);
            out.original_message = result.message;
            out.diagnostic.failure_site = "qp_kkt_factorization";
            out.diagnostic.suggested_recovery = "increase_regularization_sigma_or_rescale_problem";
        }
    } else if (out.resolved_engine == "milp" || out.resolved_engine == "miqp") {
        auto milp_options = options.milp_options;
        milp_options.deadline = options.lp_options.deadline;
        const auto milp_res = milp::solve(model, milp_options);
        result.status = milp_res.status;
        result.message = milp_res.message;
        out.nodes_explored = milp_res.nodes_explored;
        out.lp_iterations = milp_res.lp_iterations;
        out.best_bound = milp_res.best_bound;
        out.relative_gap = milp_res.relative_gap;
        out.cuts_generated = milp_res.cuts_generated;
        out.heuristics_found = milp_res.heuristics_found;
        out.diagnostic.condition_estimate = milp_res.condition_estimate;
        if (milp_res.status == lp::reference::SolveStatus::unsupported) {
            out.diagnostic.failure_site = "milp_node_lp_capacity";
            out.diagnostic.suggested_recovery =
                "use_a_model_within_dense_node_lp_limits_or_implement_sparse_node_lp";
        }
        out.ml_requested = milp_res.ml_requested;
        out.ml_model_loaded = milp_res.ml_model_loaded;
        out.ml_scoring_calls = milp_res.ml_telemetry.scored_nodes;
        out.ml_candidates_scored = milp_res.ml_telemetry.candidates_scored;
        out.ml_fallback_nodes = milp_res.ml_telemetry.fallback_nodes;
        out.ml_maximum_candidate_count =
            milp_res.ml_telemetry.maximum_candidate_count;
        out.ml_fallback_reason = milp_res.ml_fallback_reason;

        if (result.status == lp::reference::SolveStatus::optimal) {
            out.original_primal = milp_res.primal;
            out.original_objective = milp_res.objective;
            result.primal = milp_res.primal;
            result.objective = milp_res.objective;

            verify::Candidate candidate{out.original_primal, out.original_objective};
            out.primal_report = verify::verify_primal(model, candidate);
            out.original_verified = out.primal_report.passed;
            out.canonical_verified = true;
            const auto viol_desc = format_violation(out.primal_report);
            out.original_message = out.original_verified
                                       ? "original primal verified"
                                       : ("original primal rejected: " + viol_desc);
            if (!out.original_verified) {
                result.status = lp::reference::SolveStatus::numerical_failure;
                result.message = "original-model verification failed: " + viol_desc;
            }
        } else if (result.status == lp::reference::SolveStatus::infeasible ||
                   result.status == lp::reference::SolveStatus::unbounded) {
            out.canonical_verified = true;
            out.original_message = "original primal not applicable";
        } else {
            out.original_message = "original primal not applicable";
        }
    } else {
        const auto sparse_canonical =
            transform::sparse_canonicalize(model, /*relax_integrality=*/true);
        if (stop_after_deadline("LP canonicalization")) return;
        auto working_model = sparse_canonical;

        bool presolve_applied = false, scaling_applied = false;
        presolve::PresolveResult presolve_res;
        scale::RuizScalers scalers;

        if (options.enable_presolve) {
            presolve::PresolveOptions popts;
            popts.max_passes = options.max_presolve_passes;
            popts.deadline = options.lp_options.deadline;
            presolve_res = presolve::presolve(sparse_canonical, popts);
            if (stop_after_deadline("LP presolve")) return;
            if (presolve_res.status == lp::reference::SolveStatus::infeasible ||
                presolve_res.status == lp::reference::SolveStatus::unbounded) {
                result.status = presolve_res.status;
                result.message = presolve_res.message;
            } else {
                working_model = presolve_res.model;
                presolve_applied = true;
            }
        }

        if (result.status != lp::reference::SolveStatus::infeasible &&
            result.status != lp::reference::SolveStatus::unbounded && options.enable_scale &&
            working_model.matrix.rows > 0 && working_model.matrix.columns > 0) {
            scale::RuizOptions ropts;
            ropts.max_iterations = options.ruiz_iterations;
            ropts.deadline = options.lp_options.deadline;
            scalers = scale::equilibrate(working_model, ropts);
            if (stop_after_deadline("LP scaling")) return;
            scaling_applied = true;
        }

        if (result.status != lp::reference::SolveStatus::infeasible &&
            result.status != lp::reference::SolveStatus::unbounded) {
            if (working_model.matrix.rows == 0 || working_model.matrix.columns == 0) {
                result.status = lp::reference::SolveStatus::optimal;
                result.primal.assign(working_model.matrix.columns, 0.0);
                result.dual.assign(working_model.matrix.rows, 0.0);
                result.objective = 0.0;
            } else {
                const auto canonical = working_model.to_dense();
                if (stop_after_deadline("dense canonical model conversion")) return;
                if (out.resolved_engine == "ipm") {
                    // AP-1 (PS R4): interior-point engine. Same canonical path,
                    // presolve, scaling and dual-gated verification as the
                    // simplex engines; crossover converts the interior optimum
                    // into a certified vertex basis.
                    lp::interior::Options ipm_opts;
                    ipm_opts.iteration_limit = 100;
                    ipm_opts.deadline = options.lp_options.deadline;
                    if (options.lp_options.iteration_limit > 0 &&
                        options.lp_options.iteration_limit != 10000) {
                        ipm_opts.iteration_limit =
                            std::min<std::size_t>(options.lp_options.iteration_limit, 500);
                    }
                    // Documented engine fallback policy, the same contract the
                    // dual engine applies to an unusable warm start: an IPM
                    // that cannot certify a solution (numerical failure, or an
                    // uncertified status such as an iteration limit) falls back
                    // to the reference primal simplex on the same canonical
                    // model, with honest telemetry. IPM robustness/fallback is
                    // exactly the Lustig-Marsten-Shanno (1992) concern; a
                    // fallback keeps `--engine ipm` a solve-or-certify engine
                    // instead of a source of uncertified answers.
                    bool ipm_certified = false;
                    std::string ipm_failure;
                    lp::interior::Result ipm_res;
                    try {
                        ipm_res = lp::interior::solve(working_model, ipm_opts);
                        ipm_certified =
                            ipm_res.status == lp::reference::SolveStatus::optimal;
                    } catch (const std::exception& e) {
                        ipm_failure = e.what();
                    }
                    if (ipm_certified) {
                        result.status = lp::reference::SolveStatus::optimal;
                        result.primal = ipm_res.primal;
                        result.dual = ipm_res.dual;
                        result.objective = ipm_res.objective;
                        result.message = ipm_res.message;
                        result.condition_estimate = ipm_res.condition_estimate;
                        out.lp_iterations = ipm_res.iterations;
                        if (ipm_res.basis_state.has_value()) {
                            basis_to_save = *ipm_res.basis_state;
                        }
                    } else {
                        result = lp::reference::solve(canonical, options.lp_options);
                        if (result.status == lp::reference::SolveStatus::optimal &&
                            result.basis.size() == canonical.matrix.rows) {
                            // Degenerate optimal bases (duplicate indices after
                            // the primal engine fixes variables at bounds) cannot
                            // seed a warm start; the dual engine's own contract is
                            // "keep the solve result, drop the warm start", so the
                            // API path mirrors it instead of discarding a verified
                            // optimum behind a thrown exception.
                            try {
                                basis_to_save =
                                    lp::dual::make_basis_state(canonical, result.basis);
                            } catch (const std::exception&) {
                                basis_to_save.reset();
                            }
                        }
                        result.message =
                            (ipm_failure.empty()
                                 ? "ipm did not certify (" + ipm_res.message +
                                       "); reference primal revised simplex fallback"
                                 : "ipm numerical failure (" + ipm_failure +
                                       "); reference primal revised simplex fallback");
                        out.used_cold_fallback = true;
                    }
                } else if (out.resolved_engine == "dual") {
                    lp::dual::Options dual_opts;
                    dual_opts.iteration_limit = options.lp_options.iteration_limit;
                    dual_opts.deadline = options.lp_options.deadline;
                    std::optional<lp::dual::BasisState> warm_basis;
                    if (!options.warm_start_path.empty()) {
                        std::ifstream bfile(options.warm_start_path);
                        if (!bfile) {
                            throw std::invalid_argument("cannot open warm-start basis file");
                        }
                        std::string btext((std::istreambuf_iterator<char>(bfile)),
                                          std::istreambuf_iterator<char>());
                        warm_basis = lp::dual::parse_basis(btext);
                    }
                    const auto dual_res = lp::dual::solve(canonical, dual_opts, warm_basis);
                    result = dual_res.solution;
                    basis_to_save = dual_res.basis_state;
                    out.used_warm_start = dual_res.used_warm_start;
                    out.used_cold_fallback = dual_res.used_cold_fallback;
                } else {
                    result = lp::reference::solve(canonical, options.lp_options);
                    if (result.status == lp::reference::SolveStatus::optimal &&
                        result.basis.size() == canonical.matrix.rows) {
                        // Same degenerate-basis guard as the ipm-fallback path:
                        // a warm start is an optimization, never worth more than
                        // the verified optimum it would be attached to.
                        try {
                            basis_to_save = lp::dual::make_basis_state(canonical, result.basis);
                        } catch (const std::exception&) {
                            basis_to_save.reset();
                        }
                    }
                    if (result.status == lp::reference::SolveStatus::numerical_failure) {
                        // Engine fallback policy (same contract as the ipm path
                        // above, in reverse): a primal simplex that cannot certify
                        // its answer falls back to the interior-point engine on
                        // the same canonical model. IPM's normal-equation path is
                        // immune to the pivot-drift that defeats the simplex on
                        // degenerate scaled models (scsd1/scsd6). The witness check
                        // below still gates the final answer, so this cannot turn
                        // an uncertified result into a "verified" one.
                        lp::interior::Options ipm_retry;
                        ipm_retry.iteration_limit = 100;
                        ipm_retry.deadline = options.lp_options.deadline;
                        try {
                            auto ipm_r = lp::interior::solve(working_model, ipm_retry);
                            if (ipm_r.status == lp::reference::SolveStatus::optimal) {
                                result.status = lp::reference::SolveStatus::optimal;
                                result.primal = ipm_r.primal;
                                result.dual = ipm_r.dual;
                                result.objective = ipm_r.objective;
                                result.condition_estimate = ipm_r.condition_estimate;
                                result.message =
                                    "simplex numerical failure; ipm fallback optimum";
                                if (ipm_r.basis_state.has_value()) {
                                    basis_to_save = *ipm_r.basis_state;
                                } else {
                                    basis_to_save.reset();
                                }
                                out.used_cold_fallback = true;
                            }
                        } catch (const std::exception&) {
                            // keep the simplex result; honest failure stands
                        }
                    }
                }
            }
        }

        if (scaling_applied && result.status == lp::reference::SolveStatus::optimal) {
            scale::unscale_solution(scalers, result);
        }

        if (presolve_applied && result.status == lp::reference::SolveStatus::optimal) {
            result = presolve::postsolve(presolve_res.stack, result, sparse_canonical);
        }

        if (result.status == lp::reference::SolveStatus::optimal) {
            const auto Ax = sparse_canonical.multiply(result.primal);
            double max_viol = 0.0;
            for (std::size_t i = 0; i < sparse_canonical.rhs.size(); ++i) {
                max_viol = std::max(max_viol, std::abs(Ax[i] - sparse_canonical.rhs[i]));
            }
            out.canonical_report.maximum_primal_violation = max_viol;
            const double base_tol = std::max({options.lp_options.feasibility_tolerance,
                                              options.lp_options.dual_tolerance, 1e-7});
            double max_rhs = 1.0;
            for (double b_val : sparse_canonical.rhs) {
                max_rhs = std::max(max_rhs, std::abs(b_val));
            }
            out.canonical_verified = (max_viol <= base_tol * max_rhs);

            if (result.dual.size() == sparse_canonical.matrix.rows) {
                const auto aty = sparse_canonical.multiply_transpose(result.dual);
                double max_dual_viol = 0.0;
                double max_cost = 1.0;
                for (std::size_t j = 0; j < sparse_canonical.objective.size(); ++j) {
                    max_cost = std::max(max_cost, std::abs(sparse_canonical.objective[j]));
                    const double rc = sparse_canonical.objective[j] - aty[j];
                    if (-rc > max_dual_viol) {
                        max_dual_viol = -rc;
                    }
                }
                out.canonical_report.maximum_dual_violation = max_dual_viol;
                out.canonical_verified =
                    out.canonical_verified && (max_dual_viol <= base_tol * max_cost);
            }
        } else if (result.status == lp::reference::SolveStatus::infeasible ||
                   result.status == lp::reference::SolveStatus::unbounded) {
            out.canonical_verified = true;
        }

        // C-3: |c^T x - b^T y| of the canonical primal/dual witness.
        if (result.dual.size() == sparse_canonical.matrix.rows &&
            result.primal.size() == sparse_canonical.objective.size()) {
            fill_complementarity_gap(out.diagnostic, sparse_canonical.objective,
                                     sparse_canonical.rhs, result.primal, result.dual);
        }

        if ((result.status == lp::reference::SolveStatus::optimal ||
             result.status == lp::reference::SolveStatus::infeasible ||
             result.status == lp::reference::SolveStatus::unbounded) &&
            !out.canonical_verified) {
            result.status = lp::reference::SolveStatus::numerical_failure;
            result.message = "canonical witness rejected: " + out.canonical_report.message;
        }

        if (result.status == lp::reference::SolveStatus::optimal) {
            out.original_primal =
                transform::reconstruct_primal(sparse_canonical, result.primal);
            out.original_objective =
                transform::reconstruct_objective(sparse_canonical, result.objective);
            verify::Candidate candidate{out.original_primal, out.original_objective};
            out.primal_report = verify::verify_primal(model, candidate);
            out.original_verified = out.primal_report.passed;
            out.original_message = out.original_verified ? "original primal verified"
                                                         : "original primal rejected";
            if (!out.original_verified) {
                result.status = lp::reference::SolveStatus::numerical_failure;
                result.message = "original-model verification failed";
            } else if (!options.save_basis_path.empty() && basis_to_save.has_value()) {
                std::ofstream bfile(options.save_basis_path);
                if (bfile) {
                    bfile << lp::dual::serialize_basis(*basis_to_save);
                }
            }
            out.nodes_explored = 1;
            out.best_bound = out.original_objective;
            out.relative_gap = 0.0;
        } else {
            out.original_message = "original primal not applicable";
        }
        out.lp_iterations = result.phase_one_iterations + result.phase_two_iterations;
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

} // namespace

SolveResult solve_file(const std::string& path, const SolveOptions& options) {
    const auto started = Clock::now();
    const auto timed_options = with_api_deadline(options, started);
    SolveResult out;
    out.resolved_engine = options.engine;
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
        const auto model = is_lp ? io::parse_lp_file(path) : io::parse_mps(input, limits);
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
