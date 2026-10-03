#include "api_internal.hpp"
#include "engine_stages.hpp"

namespace markov_cero::api::detail {
void run_lp(const model::Model& model, const SolveOptions& options, SolveResult& out,
            lp::reference::Result& result, core::SolveContext& ctx) {
    std::optional<lp::dual::BasisState> basis_to_save;
        const auto sparse_canonical = [&] {
            core::StageScope stage(ctx, "canonicalize");
            return transform::sparse_canonicalize(model, /*relax_integrality=*/true);
        }();
        if (stop_after_deadline(ctx, options, out, result, "LP canonicalization")) return;
        if (!charge_or_fail(ctx, canonical_model_bytes(sparse_canonical), "working_model", out,
                            result))
            return;
        auto working_model = sparse_canonical;

        bool presolve_applied = false, scaling_applied = false;
        presolve::PresolveResult presolve_res;
        scale::RuizScalers scalers;

        if (options.enable_presolve) {
            core::StageScope stage(ctx, "presolve");
            presolve::PresolveOptions popts;
            popts.max_passes = options.max_presolve_passes;
            popts.deadline = options.lp_options.deadline;
            presolve_res = presolve::presolve(sparse_canonical, popts);
            if (stop_after_deadline(ctx, options, out, result, "LP presolve")) return;
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
            core::StageScope stage(ctx, "scaling");
            scale::RuizOptions ropts;
            ropts.max_iterations = options.ruiz_iterations;
            ropts.deadline = options.lp_options.deadline;
            scalers = scale::equilibrate(working_model, ropts);
            if (stop_after_deadline(ctx, options, out, result, "LP scaling")) return;
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
                // LP-01 named dimension envelope: an oversized model stays an
                // escaped length_error → work_limit (sparse-lp-path.md §3),
                // and no dense m×n canonical matrix is ever materialized.
                if (working_model.matrix.rows > 4096 ||
                    working_model.matrix.columns > 16384) {
                    throw std::length_error("lp dimension limit exceeded "
                                            "(rows<=4096, columns<=16384)");
                }
                core::StageScope solve_stage(ctx, "solve");
                if (out.resolved_engine == "ipm") {
                    run_lp_ipm_engine(working_model, options, out, result, basis_to_save);
                } else if (out.resolved_engine == "dual") {
                    lp::dual::Options dual_opts;
                    dual_opts.iteration_limit = options.lp_options.iteration_limit;
                    dual_opts.feasibility_tolerance = options.lp_options.feasibility_tolerance;
                    dual_opts.deadline = options.lp_options.deadline;
                    dual_opts.charge_bytes = &core::SolveContext::charge_hook;
                    dual_opts.release_bytes = &core::SolveContext::release_hook;
                    dual_opts.charge_user = &ctx;
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
                    lp::dual::Session local_session;
                    auto& session = options.repeated_lp_session ? *options.repeated_lp_session : local_session;
                    const auto dual_res = warm_basis ? lp::dual::solve(working_model, dual_opts, warm_basis) : session.resolve(working_model, dual_opts);
                    result = dual_res.solution;
                    basis_to_save = dual_res.basis_state;
                    out.used_warm_start = dual_res.used_warm_start;
                    out.used_cold_fallback = dual_res.used_cold_fallback;
                } else {
                    // IR-21: admit basis-factor fill into the solve-wide
                    // budget (contract §4) alongside the working-model charge.
                    auto lp_opts = options.lp_options;
                    lp_opts.charge_bytes = &core::SolveContext::charge_hook;
                    lp_opts.release_bytes = &core::SolveContext::release_hook;
                    lp_opts.charge_user = &ctx;
                    result = lp::reference::solve(working_model, lp_opts);
                    if (result.status == lp::reference::SolveStatus::optimal &&
                        result.basis.size() == working_model.matrix.rows) {
                        // Same degenerate-basis guard as the ipm-fallback path:
                        // a warm start is an optimization, never worth more than
                        // the verified optimum it would be attached to.
                        try {
                            basis_to_save = lp::dual::make_basis_state(working_model, result.basis);
                        } catch (const std::bad_alloc&) {
                            throw;
                        } catch (const std::length_error&) {
                            throw;
                        } catch (const std::exception&) {
                            basis_to_save.reset();
                        }
                    }
                    if (result.status == lp::reference::SolveStatus::numerical_failure ||
                        result.status == lp::reference::SolveStatus::iteration_limit) {
                        // Engine fallback policy (same contract as the ipm path
                        // above, in reverse): a primal simplex that cannot certify
                        // its answer — numerical breakdown or an exhausted
                        // iteration budget alike — falls back to the
                        // interior-point engine on the same canonical model.
                        // IPM's normal-equation path is immune to the pivot-drift
                        // that defeats the simplex on degenerate scaled models
                        // (scsd1/scsd6), and grow7's phase II stalls past the
                        // 10000-pivot cap that IPM clears in 100 iterations. The
                        // witness check below still gates the final answer, so
                        // this cannot turn an uncertified result into a
                        // "verified" one; if IPM does not certify, the simplex's
                        // own status and message stand unchanged.
                        const bool simplex_broke =
                            result.status == lp::reference::SolveStatus::numerical_failure;
                        const std::string simplex_failure =
                            simplex_broke ? std::string("simplex numerical failure")
                                          : result.message;
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
                                result.message = simplex_failure + "; ipm fallback optimum";
                                if (ipm_r.basis_state.has_value()) {
                                    basis_to_save = *ipm_r.basis_state;
                                } else {
                                    basis_to_save.reset();
                                }
                                out.used_cold_fallback = true;
                            }
                        } catch (const std::bad_alloc&) {
                            throw;
                        } catch (const std::length_error&) {
                            throw;
                        } catch (const std::exception&) {
                            // keep the simplex result; honest failure stands
                        }
                    }
                }
                solve_stage.set_count(out.lp_iterations != 0
                                          ? out.lp_iterations
                                          : result.phase_one_iterations +
                                                result.phase_two_iterations);
            }
        }

        if (stop_after_deadline(ctx, options, out, result, "LP solve")) return;
        {
            core::StageScope stage(ctx, "postsolve");
            if (scaling_applied && result.status == lp::reference::SolveStatus::optimal) {
                scale::unscale_solution(scalers, result);
            }

            if (presolve_applied && result.status == lp::reference::SolveStatus::optimal) {
                result = presolve::postsolve(presolve_res.stack, result, sparse_canonical);
            }
        }

        if (stop_after_deadline(ctx, options, out, result, "LP postsolve")) return;
        core::StageScope verify_stage(ctx, "verify");
        const double witness_tolerance = std::max({options.lp_options.feasibility_tolerance,
                                                   options.lp_options.dual_tolerance, 1e-8});
        out.canonical_report = verify::verify_sparse_result(sparse_canonical, result,
                                                            witness_tolerance, ctx.deadline());
        out.canonical_verified = out.canonical_report.accepted;
        out.certificate_type = out.canonical_verified ? "canonical_lp_witness" : "none";

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
            // A witness the verifier did not finish checking (the solve-wide
            // deadline expired mid-scan) is a resource stop, never a claim
            // about the model and never a numerical failure.
            const bool stopped_by_deadline = ctx.deadline().expired();
            if (stopped_by_deadline) (void)ctx.note_stop(core::StopReason::deadline_exceeded);
            result.status = stopped_by_deadline ? lp::reference::SolveStatus::resource_limit
                                                : lp::reference::SolveStatus::numerical_failure;
            result.message = stopped_by_deadline
                                 ? "solve deadline reached before canonical verification completed"
                                 : "canonical witness rejected: " + out.canonical_report.message;
        }

        if (result.status == lp::reference::SolveStatus::optimal) {
            out.row_duals.assign(model.matrix.row_count, 0.0);
            for (std::size_t i = 0; i < sparse_canonical.record.rows.size(); ++i) {
                const auto& mapping = sparse_canonical.record.rows[i];
                for (std::size_t k = 0; k < mapping.canonical_index.size(); ++k)
                    out.row_duals[i] += sparse_canonical.record.objective_sign * mapping.multiplier[k] *
                        result.dual[mapping.canonical_index[k]];
            }
            out.reduced_costs = model.objective;
            for (std::size_t j = 0; j < model.matrix.column_count; ++j)
                for (std::size_t k = model.matrix.column_start[j]; k < model.matrix.column_start[j + 1]; ++k)
                    out.reduced_costs[j] -= model.matrix.value[k] * out.row_duals[model.matrix.row_index[k]];
            out.original_primal =
                transform::reconstruct_primal(sparse_canonical, result.primal);
            out.original_objective =
                transform::reconstruct_objective(sparse_canonical, result.objective);
            verify::Candidate candidate{out.original_primal, out.original_objective};
            out.primal_report = verify::verify_primal(model, candidate, {}, {}, 1e-7,
                                                     true, ctx.deadline());
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

} // namespace markov_cero::api::detail
