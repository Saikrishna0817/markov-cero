#include "markov_cero/milp/milp_solver.hpp"

#include "markov_cero/milp/branch_node.hpp"
#include "markov_cero/milp/cuts.hpp"
#include "markov_cero/milp/heuristics.hpp"
#include "markov_cero/milp/node_lp.hpp"
#include "markov_cero/milp/strong_branching.hpp"
#ifdef MARKOV_CERO_ENABLE_ML
#include "markov_cero/milp/ml_branching/onnx_scorer.hpp"
#endif
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/model.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <queue>
#include <string>

namespace markov_cero::milp {

namespace {

// Frontier container for the sequential tree search: a heap ordered by the
// node-selection policy so top() is the policy-preferred node, plus a
// policy-independent minimum-bound query.
//
// R13/R17 soundness: under depth_first / best_bound_dive the heap front is
// the deepest / dive-score node, NOT the minimum-bound node. Reading the
// global lower bound from top() (as the previous std::priority_queue code
// did) inflates the frontier bound up to the incumbent, closes the
// optimality gap spuriously, and reports "optimal" for a SUBOPTIMAL
// incumbent (reproduced: knapsack optimum 193 reported as 171 with
// --node-selection depth-first). min_lower_bound() scans instead.
class NodeFrontier {
  public:
    explicit NodeFrontier(NodeSelection policy) : comparator_{policy} {}

    void push(std::shared_ptr<BranchNode> node) {
        if (!node) {
            return;
        }
        heap_.push_back(std::move(node));
        std::push_heap(heap_.begin(), heap_.end(), comparator_);
    }

    [[nodiscard]] bool empty() const { return heap_.empty(); }
    [[nodiscard]] std::size_t size() const { return heap_.size(); }
    [[nodiscard]] const std::shared_ptr<BranchNode>& top() const { return heap_.front(); }

    void pop() {
        std::pop_heap(heap_.begin(), heap_.end(), comparator_);
        heap_.pop_back();
    }

    // Exact minimum lower bound over the whole frontier. Under best_bound
    // ordering the heap front already is the minimum (O(1)); other policies
    // scan (frontiers under depth_first/dive stay near the search path).
    [[nodiscard]] double min_lower_bound() const {
        if (heap_.empty()) {
            return std::numeric_limits<double>::infinity();
        }
        if (comparator_.policy == NodeSelection::best_bound) {
            return heap_.front()->lower_bound;
        }
        double bound = std::numeric_limits<double>::infinity();
        for (const auto& node : heap_) {
            if (node && node->lower_bound < bound) {
                bound = node->lower_bound;
            }
        }
        return bound;
    }

  private:
    NodeComparator comparator_;
    std::vector<std::shared_ptr<BranchNode>> heap_;
};

} // namespace

Result solve(const model::Model& model, const Options& input_options) {
    const auto start_time = std::chrono::steady_clock::now();
    Options options = input_options;
    if (!options.deadline && std::isfinite(options.time_limit_seconds) &&
        options.time_limit_seconds > 0.0) {
        options.deadline = start_time + std::chrono::duration_cast<
            std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(options.time_limit_seconds));
    }
    Result result;
    try {
        model.validate();
    } catch (const std::exception& e) {
        result.status = lp::reference::SolveStatus::invalid_model;
        result.message = e.what();
        return result;
    }

    // The B&B implementation's incumbent comparisons and global-bound logic
    // are defined for minimization. Normalize maximization once at the solver
    // boundary, then convert every objective-space result back for callers.
    if (model.objective_sense == model::ObjectiveSense::maximize) {
        auto normalized = model;
        normalized.objective_sense = model::ObjectiveSense::minimize;
        normalized.objective_offset = -normalized.objective_offset;
        for (auto& c : normalized.objective) c = -c;
        for (auto& q : normalized.quadratic_matrix.value) q = -q;
        auto normalized_result = solve(normalized, options);
        if (normalized_result.status == lp::reference::SolveStatus::optimal ||
            !normalized_result.primal.empty())
            normalized_result.objective = -normalized_result.objective;
        if (std::isfinite(normalized_result.best_bound))
            normalized_result.best_bound = -normalized_result.best_bound;
        normalized_result.message = "maximization normalized internally; " +
                                    normalized_result.message;
        return normalized_result;
    }

    // Keep scorer ownership local to this solve. A process-global hook races
    // when two API clients solve different MILPs concurrently.
    const IBranchingScorer* branching_scorer = nullptr;
    result.ml_requested = options.branching_strategy == BranchingStrategy::ml_gnn;
    if (result.ml_requested) {
        result.ml_fallback_reason = "no_ml_branching_node_observed";
    }
#ifdef MARKOV_CERO_ENABLE_ML
    // W2/D-04: install the ONNX scorer when the caller asked for ml_gnn and
    // the model file exists. Any failure here degrades silently to
    // pseudo_cost (LOCKED activation contract: ML is opt-in, never fatal).
    std::unique_ptr<IBranchingScorer> owned_scorer;
    if (options.branching_strategy == BranchingStrategy::ml_gnn) {
        const char* model_path = std::getenv("MARKOV_CERO_ML_MODEL");
        const std::string default_path = "data/ml_models/branching_scorer.onnx";
        try {
            owned_scorer = std::make_unique<ml::OnnxBranchingScorer>(
                model_path != nullptr ? std::string(model_path) : default_path);
            branching_scorer = owned_scorer.get();
            result.ml_model_loaded = true;
            result.ml_fallback_reason = "no_ml_branching_node_observed";
        } catch (const std::exception& e) {
            // silent fallback: scorer stays null; select_branching_variable
            // takes the pseudo-cost path (and the ml_gnn candidates.size()>200
            // gate would not have passed on small instances anyway).
            result.ml_fallback_reason = std::string("model_load_failed: ") + e.what();
        }
    }
    // W2/D-05: optional strong-branching training log (opened on demand).
    std::ofstream sb_log_stream;
    if (const char* sb_path = std::getenv("MARKOV_CERO_SB_LOG"); sb_path != nullptr) {
        sb_log_stream.open(sb_path, std::ios::binary | std::ios::app);
    }
    std::ofstream* sb_log_file = sb_log_stream.is_open() ? &sb_log_stream : nullptr;
#else
    if (result.ml_requested) {
        result.ml_fallback_reason = "ml_not_compiled";
    }
#endif

    // Check if model is purely continuous
    bool has_discrete = false;
    for (const auto type : model.variable_type) {
        if (type != model::VariableType::continuous) {
            has_discrete = true;
            break;
        }
    }

    if (!has_discrete) {
        if (model.has_quadratic_objective) {
            const auto qp = qp::make_quadratic_model(model);
            qp::QpOptions qopts;
            qopts.max_iterations = options.max_iterations;
            qopts.absolute_tolerance = options.feasibility_tolerance;
            qopts.relative_tolerance = options.feasibility_tolerance;
            const auto qpres = qp::solve_qp(qp, qopts);
            if (qpres.status == qp::QpStatus::optimal) {
                result.status = lp::reference::SolveStatus::optimal;
                result.primal = qpres.x;
                result.objective = qpres.objective_value;
                result.best_bound = qpres.objective_value;
                result.relative_gap = 0.0;
                result.message = "pure continuous QP solved to optimality";
            } else if (qpres.status == qp::QpStatus::primal_infeasible) {
                result.status = lp::reference::SolveStatus::infeasible;
                result.message = "pure continuous QP is infeasible";
            } else {
                result.status = lp::reference::SolveStatus::numerical_failure;
                result.message = "QP solve failed";
            }
            result.lp_iterations = qpres.iterations;
            result.condition_estimate = qpres.condition_estimate;
            result.nodes_explored = 1;
            const auto elapsed = std::chrono::steady_clock::now() - start_time;
            result.runtime_ms =
                std::chrono::duration<double, std::milli>(elapsed).count();
            return result;
        }
        // Pure continuous LP shortcut
        const auto canon =
            transform::sparse_canonicalize(model, /*relax_integrality=*/false);
        const auto dense = canon.to_dense();
        lp::reference::Options ropts;
        ropts.iteration_limit = options.max_iterations;
        ropts.feasibility_tolerance = options.feasibility_tolerance;
        const auto lpres = lp::reference::solve(dense, ropts);

        result.status = lpres.status;
        result.lp_iterations =
            lpres.phase_one_iterations + lpres.phase_two_iterations;
        result.condition_estimate = lpres.condition_estimate;
        result.nodes_explored = 1;
        if (result.status == lp::reference::SolveStatus::optimal) {
            result.primal = transform::reconstruct_primal(canon, lpres.primal);
            result.objective =
                transform::reconstruct_objective(canon, lpres.objective);
            result.best_bound = result.objective;
            result.relative_gap = 0.0;
            result.message = "pure continuous LP solved to optimality";
        } else {
            result.message = lpres.message;
        }
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        result.runtime_ms =
            std::chrono::duration<double, std::milli>(elapsed).count();
        return result;
    }

    // Initialize search state
    std::size_t next_node_id = 1;
    double best_upper_bound = std::numeric_limits<double>::infinity();
    double best_lower_bound = -std::numeric_limits<double>::infinity();
    std::vector<double> best_primal;
    std::vector<VariablePseudoCost> pseudo_costs(model.matrix.column_count);

    model::Model root_model = model;

    // 1. Solve Root Continuous LP Relaxation
    const auto root_lp = solve_node_relaxation(root_model, options, std::nullopt);
    result.lp_iterations += root_lp.iterations;
    result.condition_estimate = root_lp.condition_estimate;
    result.nodes_explored = 1;

    if (root_lp.status == lp::reference::SolveStatus::infeasible) {
        result.status = lp::reference::SolveStatus::infeasible;
        result.message = "root continuous relaxation is infeasible";
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
        return result;
    }
    if (root_lp.status != lp::reference::SolveStatus::optimal) {
        result.status = root_lp.status;
        if (std::isfinite(root_lp.lower_bound))
            result.best_bound = root_lp.lower_bound;
        result.message = root_lp.message.empty()
                             ? std::string("root continuous relaxation failed: ") +
                                   lp::reference::to_string(root_lp.status)
                             : "root continuous relaxation failed: " + root_lp.message;
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
        return result;
    }

    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.best_bound = root_lp.lower_bound;
        result.message = "wall-clock deadline reached during root relaxation";
        if (check_integer_feasibility(root_model, root_lp.primal,
                                      options.feasibility_tolerance,
                                      options.integrality_tolerance)) {
            result.primal = root_lp.primal;
            result.objective = root_lp.objective;
            result.relative_gap = std::abs(root_lp.objective - root_lp.lower_bound) /
                                  std::max(1.0, std::abs(root_lp.objective));
        }
        result.runtime_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    best_lower_bound = root_lp.lower_bound;

    // Check if root continuous solution is integer feasible
    if (check_integer_feasibility(root_model, root_lp.primal, options.feasibility_tolerance,
                                  options.integrality_tolerance)) {
        result.primal = root_lp.primal;
        result.objective = root_lp.objective;
        result.best_bound = root_lp.lower_bound;
        result.relative_gap = std::abs(root_lp.objective - root_lp.lower_bound) /
                              std::max(1.0, std::abs(root_lp.objective));
        result.status = result.relative_gap <= options.relative_gap_tolerance
                            ? lp::reference::SolveStatus::optimal
                            : lp::reference::SolveStatus::iteration_limit;
        result.message = result.status == lp::reference::SolveStatus::optimal
                             ? "root relaxation integer feasible and dual gap certified"
                             : "root relaxation integer feasible but dual gap is not closed";
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
        return result;
    }

    // 2. Run Primal Heuristics at Root
    if (options.enable_heuristics) {
        const auto hr = simple_rounding(root_model, root_lp.primal, options.feasibility_tolerance,
                                        options.integrality_tolerance);
        if (hr.found && hr.objective < best_upper_bound) {
            best_upper_bound = hr.objective;
            best_primal = hr.primal;
            ++result.heuristics_found;
        }

        const auto fp =
            feasibility_pump(root_model, root_lp.primal, options.max_pump_iterations,
                             options.feasibility_tolerance, options.integrality_tolerance);
        if (fp.found && fp.objective < best_upper_bound) {
            best_upper_bound = fp.objective;
            best_primal = fp.primal;
            ++result.heuristics_found;
        }
    }

    // 3. Generate Gomory Mixed-Integer and MIR Cuts at Root
    std::optional<lp::dual::BasisState> current_basis = root_lp.basis;
    std::vector<double> current_primal = root_lp.primal;
    std::vector<double> current_row_dual = root_lp.row_dual;
    double current_obj = root_lp.objective;
    std::vector<Cut> root_cut_list;

    if (options.enable_cuts && root_lp.basis.has_value()) {
        try {
            const auto canon =
                transform::sparse_canonicalize(root_model, /*relax_integrality=*/true);
            std::vector<Cut> cuts = generate_gomory_cuts(root_model, current_primal, canon,
                                                         *root_lp.basis, options.max_cut_rounds);
            if (options.enable_mir_cuts) {
                const auto mir_cuts = generate_mir_cuts(root_model, current_primal, canon,
                                                        *root_lp.basis, options.max_cut_rounds);
                cuts.insert(cuts.end(), mir_cuts.begin(), mir_cuts.end());
            }
            cuts = filter_cuts(std::move(cuts), options.max_cut_rounds);
            if (!cuts.empty()) {
                add_cuts_to_model(root_model, cuts);
                root_cut_list = cuts;
                result.cuts_generated += cuts.size();

                // Re-solve root LP with cuts
                const auto cut_lp =
                    solve_node_relaxation(root_model, options, root_lp.basis);
                result.lp_iterations += cut_lp.iterations;
                if (cut_lp.status == lp::reference::SolveStatus::optimal) {
                    current_primal = cut_lp.primal;
                    current_row_dual = cut_lp.row_dual;
                    current_obj = cut_lp.objective;
                    current_basis = cut_lp.basis;
                    best_lower_bound = std::max(best_lower_bound, cut_lp.lower_bound);

                    if (check_integer_feasibility(root_model, current_primal,
                                                  options.feasibility_tolerance,
                                                  options.integrality_tolerance)) {
                        if (current_obj < best_upper_bound) {
                            best_upper_bound = current_obj;
                            best_primal = current_primal;
                        }
                    }
                }
            }
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& e) {
            if (std::getenv("MARKOV_NODE_DEBUG")) {
                std::fprintf(stderr, "[milp] root cuts failed: %s\n", e.what());
            }
        }
    }

    // Check if root cuts closed the optimality gap
    if (!best_primal.empty() && best_lower_bound > -std::numeric_limits<double>::infinity()) {
        const double gap = std::abs(best_upper_bound - best_lower_bound) /
                           std::max(1.0, std::abs(best_upper_bound));
        if (gap <= options.relative_gap_tolerance) {
            result.status = lp::reference::SolveStatus::optimal;
            result.primal = best_primal;
            result.objective = best_upper_bound;
            result.best_bound = best_lower_bound;
            result.relative_gap = gap;
            result.message = "optimality gap closed at root node";
            const auto elapsed = std::chrono::steady_clock::now() - start_time;
            result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
            return result;
        }
    }

    // 4. Root strong branching is only useful for strategies that consume its
    // scores. Running it unconditionally made an ordinary pseudo-cost solve
    // spend O(number of fractional variables) full LP re-solves before its
    // first B&B node (the supply-chain case spent >90s here). Bound the root
    // probe set; later strong-branching decisions remain governed by strategy.
    const bool root_uses_strong_branching =
        options.branching_strategy == BranchingStrategy::strong_branching ||
        options.branching_strategy == BranchingStrategy::reliability;
    if (options.enable_strong_branching && root_uses_strong_branching &&
        current_basis.has_value()) {
        try {
            StrongBranchingOptions sb_opts;
            sb_opts.integrality_tolerance = options.integrality_tolerance;
            sb_opts.feasibility_tolerance = options.feasibility_tolerance;
            sb_opts.deadline = options.deadline;
#ifdef MARKOV_CERO_ENABLE_ML
            // During data collection, keep probe-only gains out of the
            // pseudo-cost state. Training features should reflect only
            // information available to the ML policy from solved tree nodes.
            sb_opts.update_pseudo_costs = sb_log_file == nullptr;
#else
            sb_opts.update_pseudo_costs = true;
#endif
            // The root pass initializes pseudo-costs only; feature/label
            // records are emitted for per-node strong branching below.
            sb_opts.max_candidates = 4;
            const auto sb_res = evaluate_strong_branching(root_model, current_primal, current_obj,
                                                          current_basis, sb_opts, &pseudo_costs);
            if (sb_res.deadline_reached) {
                result.status = lp::reference::SolveStatus::resource_limit;
                result.best_bound = best_lower_bound;
                if (std::isfinite(best_upper_bound)) {
                    result.primal = best_primal;
                    result.objective = best_upper_bound;
                    result.relative_gap = std::max(0.0, best_upper_bound - best_lower_bound) /
                                          std::max(1.0, std::abs(best_upper_bound));
                }
                result.message = "wall-clock deadline reached during root strong branching";
                result.runtime_ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start_time).count();
                return result;
            }
            if (std::getenv("MARKOV_NODE_DEBUG")) {
                std::fprintf(stderr, "[milp] root strong-branch candidates=%zu reductions=%zu both_infeasible=%d\n",
                             sb_res.candidates.size(), sb_res.domain_reductions.size(),
                             sb_res.subproblem_infeasible ? 1 : 0);
            }

            if (sb_res.subproblem_infeasible) {
                result.status = lp::reference::SolveStatus::infeasible;
                result.message = "proven infeasible by strong branching at root";
                const auto elapsed = std::chrono::steady_clock::now() - start_time;
                result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
                return result;
            }

            // Apply discovered domain reductions to root model. These bounds
            // are logical consequences of an infeasible branch, but they can
            // invalidate the root LP point and basis. Re-solve before the root
            // node is enqueued; using the stale relaxation can make both
            // branches appear infeasible and incorrectly prune a feasible MIP.
            bool root_domain_changed = false;
            for (const auto& dr : sb_res.domain_reductions) {
                if (dr.variable_index < root_model.matrix.column_count) {
                    if (std::getenv("MARKOV_NODE_DEBUG"))
                        std::fprintf(stderr, "[milp] root reduction var=%zu new_lb=%d:%.12g new_ub=%d:%.12g\n",
                                     dr.variable_index, dr.new_lower.is_finite() ? 1 : 0,
                                     dr.new_lower.value, dr.new_upper.is_finite() ? 1 : 0,
                                     dr.new_upper.value);
                    if (dr.new_lower.is_finite()) {
                        root_domain_changed = root_domain_changed ||
                            !root_model.variable_lower[dr.variable_index].is_finite() ||
                            dr.new_lower.value > root_model.variable_lower[dr.variable_index].value;
                        root_model.variable_lower[dr.variable_index] = dr.new_lower;
                    }
                    if (dr.new_upper.is_finite()) {
                        root_domain_changed = root_domain_changed ||
                            !root_model.variable_upper[dr.variable_index].is_finite() ||
                            dr.new_upper.value < root_model.variable_upper[dr.variable_index].value;
                        root_model.variable_upper[dr.variable_index] = dr.new_upper;
                    }
                }
            }
            if (root_domain_changed) {
                const auto reduced_root_lp =
                    solve_node_relaxation(root_model, options, current_basis);
                result.lp_iterations += reduced_root_lp.iterations;
                if (reduced_root_lp.status != lp::reference::SolveStatus::optimal) {
                    result.status = reduced_root_lp.status;
                    if (std::isfinite(reduced_root_lp.lower_bound))
                        result.best_bound = reduced_root_lp.lower_bound;
                    result.message = reduced_root_lp.status == lp::reference::SolveStatus::infeasible
                        ? "root relaxation is infeasible after certified strong-branching reductions"
                        : "root relaxation failed after strong-branching reductions: " +
                              reduced_root_lp.message;
                    const auto elapsed = std::chrono::steady_clock::now() - start_time;
                    result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
                    return result;
                }
                current_primal = reduced_root_lp.primal;
                current_row_dual = reduced_root_lp.row_dual;
                current_obj = reduced_root_lp.objective;
                current_basis = reduced_root_lp.basis;
                best_lower_bound = std::max(best_lower_bound, reduced_root_lp.lower_bound);
                if (std::getenv("MARKOV_NODE_DEBUG")) {
                    std::fprintf(stderr, "[milp] re-solved root obj=%.12g x=", current_obj);
                    for (double v : current_primal) std::fprintf(stderr, " %.12g", v);
                    std::fprintf(stderr, "\n");
                }
            }
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& e) {
            if (std::getenv("MARKOV_NODE_DEBUG")) {
                std::fprintf(stderr, "[milp] root strong branching failed: %s\n", e.what());
            }
        }
    }

    // 5. Initialize Active Node Priority Queue (policy-ordered heap with a
    // policy-independent min-bound query — see NodeFrontier).
    NodeFrontier queue{options.node_selection};

    // R13/R17 soundness bookkeeping. `unsolved_node_lps` counts nodes whose
    // relaxation could not be certified; `min_unsolved_bound` is the weakest
    // bound among them, so the honest global lower bound is
    // min(best_lower_bound, min_unsolved_bound) whenever the counter is nonzero.
    std::size_t unsolved_node_lps = 0;
    double min_unsolved_bound = std::numeric_limits<double>::infinity();

    auto root_node = std::make_shared<BranchNode>();
    root_node->id = 0;
    root_node->parent_id = 0;
    root_node->depth = 0;
    root_node->lower_bound = best_lower_bound;
    root_node->variable_lower = root_model.variable_lower;
    root_node->variable_upper = root_model.variable_upper;
    root_node->warm_basis = current_basis;
    queue.push(root_node);

    // 6. Tree Search Loop
    std::string stop_reason;
    while (!queue.empty() && result.nodes_explored < options.max_nodes) {
        const auto now = std::chrono::steady_clock::now();
        if (options.deadline && now >= *options.deadline) {
            stop_reason = "time limit reached";
            break;
        }

        auto node = queue.top();
        queue.pop();

        // Bound pruning
        if (node->lower_bound >= best_upper_bound - options.absolute_gap_tolerance) {
            continue;
        }

        // Evaluate Node LP relaxation if not root
        NodeLpResult node_lp_res;
        model::Model node_model = root_model;
        if (node->id == 0) {
            node_lp_res.status = lp::reference::SolveStatus::optimal;
            node_lp_res.primal = current_primal;
            node_lp_res.row_dual = current_row_dual;
            node_lp_res.objective = current_obj;
            node_lp_res.lower_bound = best_lower_bound;
            node_lp_res.basis = current_basis;
        } else {
            node_model.variable_lower = node->variable_lower;
            node_model.variable_upper = node->variable_upper;
            if (!node->local_cuts.empty()) {
                add_cuts_to_model(node_model, node->local_cuts);
            }

            node_lp_res =
                solve_node_relaxation(node_model, options, node->warm_basis);
            result.lp_iterations += node_lp_res.iterations;
            ++result.nodes_explored;
            if (std::getenv("MARKOV_NODE_DEBUG")) {
                std::fprintf(stderr, "[milp] node=%zu lp_status=%s obj=%.12g x=",
                             node->id, lp::reference::to_string(node_lp_res.status),
                             node_lp_res.objective);
                for (double v : node_lp_res.primal) std::fprintf(stderr, " %.12g", v);
                std::fprintf(stderr, "\n");
            }

            if (node_lp_res.status == lp::reference::SolveStatus::infeasible) {
                // Sound prune: the node's relaxation is empty, so no feasible
                // integer point can live below it.
                continue;
            }
            if (node_lp_res.status != lp::reference::SolveStatus::optimal) {
                // R13/R17 soundness: an *unsolved* node (numerical failure or
                // iteration limit) is NOT prunable — it still carries its
                // inherited bound, and discarding it lets the search exhaust
                // the tree and claim optimality it never proved (the classic
                // bogus "gap 0" on degenerate big-M instances).
                //
                // Retry once cold on the robust primal path, then keep the node
                // open and remember that optimality was not established.
                if (std::getenv("MARKOV_NODE_DEBUG")) {
                    std::fprintf(stderr,
                                 "[milp] unsolved node id=%zu depth=%zu bound=%.6f status=%d "
                                 "warm=%d retried=%d\n",
                                 node->id, node->depth, node->lower_bound,
                                 static_cast<int>(node_lp_res.status),
                                 node->warm_basis.has_value() ? 1 : 0, (int)node->lp_failures);
                }
                if (node->lp_failures == 0) {
                    // First failure: retry the node cold on the robust primal
                    // path. Only a node that fails *after* the retry stays open,
                    // so a transient warm-start breakdown does not by itself
                    // forfeit the optimality proof.
                    node->lp_failures = 1;
                    node->warm_basis.reset();
                    queue.push(node);
                } else {
                    ++unsolved_node_lps;
                    min_unsolved_bound = std::min(min_unsolved_bound, node->lower_bound);
                }
                continue;
            }

            const double parent_bound = node->lower_bound;
            node->lower_bound = std::max(node->lower_bound, node_lp_res.lower_bound);

            // Bound pruning after solving node LP
            if (node_lp_res.lower_bound >= best_upper_bound - options.absolute_gap_tolerance) {
                continue;
            }

            // Update pseudo-cost of parent branch
            if (node->depth > 0) {
                const std::size_t b_var = node->branch_variable;
                const double delta_z = node_lp_res.lower_bound - parent_bound;
                const double frac = node->branch_value - std::floor(node->branch_value);
                if (node->is_down_branch) {
                    pseudo_costs[b_var].record_down(delta_z, frac);
                } else {
                    pseudo_costs[b_var].record_up(delta_z, 1.0 - frac);
                }
            }
        }

        // In-tree cut separation: frequency-gated, pool-deduped, bounded rounds (ED-005).
        // RW-1 safety valves: (a) skip expensive nodes (LP iterations above
        // separation_max_node_iterations) — re-separating on hard nodes costs more
        // LP re-solve time than the pruning recovers; (b) skip near-integral nodes
        // whose max fractionality is tiny — branching closes them cheaply while cut
        // rows would inflate every descendant LP (both measured on flugpl).
        double max_fractionality = 0.0;
        for (const auto fv : find_fractional_variables(node_lp_res.primal,
                                                       root_model.variable_type,
                                                       options.integrality_tolerance)) {
            const double f = node_lp_res.primal[fv] - std::floor(node_lp_res.primal[fv]);
            max_fractionality = std::max(max_fractionality, std::min(f, 1.0 - f));
        }
        // Per-node budget: the cut round may spend at most
        // separation_lp_budget_factor x this node's base LP iterations.
        const double node_lp_budget =
            static_cast<double>(node_lp_res.iterations) * options.separation_lp_budget_factor;
        if (options.enable_cuts && options.separation_frequency > 0 && node->id != 0 &&
            node_lp_res.basis.has_value() &&
            node_lp_res.iterations <= options.separation_max_node_iterations &&
            max_fractionality >= options.separation_min_max_fractionality &&
            result.nodes_explored % options.separation_frequency == 0) {
            try {
                const std::size_t rounds =
                    std::min(options.max_in_tree_cut_rounds, options.max_cut_rounds);
                for (std::size_t round = 0; round < rounds; ++round) {
                    const auto fractional = find_fractional_variables(
                        node_lp_res.primal, root_model.variable_type,
                        options.integrality_tolerance);
                    if (fractional.empty()) {
                        break;
                    }

                    const auto canon = transform::sparse_canonicalize(
                        node_model, /*relax_integrality=*/true);
                    // In-tree budget: fewer, stronger cuts per node. Generating the
                    // full root allowance at every node inflates LP size (and the
                    // canonicalization + re-solve cost) faster than it prunes —
                    // the measured flugpl regression (RW-1 tuning, ED-005).
                    std::vector<Cut> candidates = generate_gomory_cuts(
                        node_model, node_lp_res.primal, canon, *node_lp_res.basis,
                        options.max_in_tree_cuts_per_node);
                    if (options.enable_mir_cuts) {
                        const auto mir_cuts = generate_mir_cuts(
                            node_model, node_lp_res.primal, canon, *node_lp_res.basis,
                            options.max_in_tree_cuts_per_node);
                        candidates.insert(candidates.end(), mir_cuts.begin(), mir_cuts.end());
                    }
                    candidates = filter_cuts(std::move(candidates),
                                             options.max_in_tree_cuts_per_node);

                    std::vector<Cut> fresh;
                    fresh.reserve(candidates.size());
                    for (auto& cut : candidates) {
                        double lhs = 0.0;
                        for (std::size_t j = 0; j < cut.coefficients.size(); ++j) {
                            lhs += cut.coefficients[j] * node_lp_res.primal[j];
                        }
                        if (cut.rhs - lhs < 1e-4) {
                            continue;
                        }
                        bool pooled = false;
                        for (const auto& existing : root_cut_list) {
                            if (compute_cosine_similarity(cut, existing) > 0.95) {
                                pooled = true;
                                break;
                            }
                        }
                        if (!pooled) {
                            for (const auto& existing : node->local_cuts) {
                                if (compute_cosine_similarity(cut, existing) > 0.95) {
                                    pooled = true;
                                    break;
                                }
                            }
                        }
                        if (!pooled) {
                            fresh.push_back(std::move(cut));
                        }
                    }
                    if (fresh.empty()) {
                        break;
                    }

                    add_cuts_to_model(node_model, fresh);
                    node->local_cuts.insert(node->local_cuts.end(), fresh.begin(), fresh.end());
                    result.cuts_generated += fresh.size();

                    const auto cut_lp =
                        solve_node_relaxation(node_model, options, node_lp_res.basis);
                    result.lp_iterations += cut_lp.iterations;
                    if (cut_lp.status != lp::reference::SolveStatus::optimal) {
                        // Unverified cuts must not propagate to children: revert this round.
                        node->local_cuts.resize(node->local_cuts.size() - fresh.size());
                        result.cuts_generated -= fresh.size();
                        break;
                    }
                    // Budget enforcement: if the re-solve consumed more than the
                    // node's allowance, keep the (verified) cuts but stop separating
                    // here. Prevents unbounded LP-work amplification on instances
                    // where in-tree cuts do not pay for themselves.
                    if (static_cast<double>(cut_lp.iterations) > node_lp_budget) {
                        break;
                    }
                    node_lp_res = cut_lp;
                    node->lower_bound = std::max(node->lower_bound, cut_lp.lower_bound);
                    if (cut_lp.lower_bound >= best_upper_bound - options.absolute_gap_tolerance) {
                        break;
                    }
                }
            } catch (const std::bad_alloc&) {
                throw;
            } catch (const std::exception& e) {
                if (std::getenv("MARKOV_NODE_DEBUG")) {
                    std::fprintf(stderr, "[milp] in-tree cut separation failed: %s\n", e.what());
                }
            }

            if (node->local_cuts.size() > options.max_pool_cuts) {
                std::stable_sort(node->local_cuts.begin(), node->local_cuts.end(),
                                 [](const Cut& a, const Cut& b) {
                                     return a.violation > b.violation;
                                 });
                node->local_cuts.resize(options.max_pool_cuts);
            }
        }

        // Check integer feasibility of node solution
        const auto fractional_vars = find_fractional_variables(
            node_lp_res.primal, root_model.variable_type, options.integrality_tolerance);

        if (fractional_vars.empty()) {
            // Integer feasible incumbent found!
            if (std::getenv("MARKOV_NODE_DEBUG"))
                std::fprintf(stderr, "[milp] node=%zu integer feasible obj=%.12g\n",
                             node->id, node_lp_res.objective);
            if (node_lp_res.objective < best_upper_bound) {
                best_upper_bound = node_lp_res.objective;
                best_primal = node_lp_res.primal;
            }
            continue;
        }

        // Try quick simple rounding on fractional point
        if (options.enable_heuristics && result.nodes_explored % 5 == 0) {
            node_model.variable_lower = node->variable_lower;
            node_model.variable_upper = node->variable_upper;
            const auto hr =
                simple_rounding(node_model, node_lp_res.primal, options.feasibility_tolerance,
                                options.integrality_tolerance);
            if (hr.found && hr.objective < best_upper_bound) {
                best_upper_bound = hr.objective;
                best_primal = hr.primal;
                ++result.heuristics_found;
            }
        }

        // 7. Branching Variable Selection
        std::size_t branch_var = root_model.matrix.column_count;
        model::Model current_node_model = root_model;
        current_node_model.variable_lower = node->variable_lower;
        current_node_model.variable_upper = node->variable_upper;
        if (options.branching_strategy == BranchingStrategy::strong_branching &&
            node_lp_res.basis.has_value()) {
            try {
                StrongBranchingOptions sb_opts;
                sb_opts.integrality_tolerance = options.integrality_tolerance;
                sb_opts.feasibility_tolerance = options.feasibility_tolerance;
                sb_opts.deadline = options.deadline;
#ifdef MARKOV_CERO_ENABLE_ML
                // Strong-branch probes create labels, not prior observations
                // for the feature vector. Do not inject these labels into
                // pseudo-cost history while collecting training records.
                sb_opts.update_pseudo_costs = sb_log_file == nullptr;
#else
                sb_opts.update_pseudo_costs = true;
#endif
                // The graph features must describe the state available before
                // this node's strong-branch probes. Those probes update the
                // pseudo-cost table with the very gains used as labels; using
                // the updated table below would leak the target into training.
                const auto feature_pseudo_costs = pseudo_costs;
                const auto sb_res = evaluate_strong_branching(
                    current_node_model, node_lp_res.primal, node_lp_res.objective,
                    node_lp_res.basis, sb_opts, &pseudo_costs);
                if (sb_res.deadline_reached) {
                    queue.push(node);
                    stop_reason = "time limit reached during strong branching";
                    break;
                }
                if (std::getenv("MARKOV_NODE_DEBUG")) {
                    std::fprintf(stderr, "[milp] node=%zu strong-branch candidates=%zu reductions=%zu both_infeasible=%d\n",
                                 node->id, sb_res.candidates.size(),
                                 sb_res.domain_reductions.size(),
                                 sb_res.subproblem_infeasible ? 1 : 0);
                    for (const auto& c : sb_res.candidates) {
                        std::fprintf(stderr, "[milp] candidate=%zu down_inf=%d up_inf=%d down=%.12g up=%.12g\n",
                                     c.variable_index, c.is_down_infeasible ? 1 : 0,
                                     c.is_up_infeasible ? 1 : 0,
                                     c.down_degradation, c.up_degradation);
                    }
                }
                if (sb_res.subproblem_infeasible) {
                    continue; // Prune node
                }
                branch_var = sb_res.best_variable;
#ifdef MARKOV_CERO_ENABLE_ML
                // W2/D-05: strong-branching training log. When the collector
                // sets MARKOV_CERO_SB_LOG, append one record per node: the
                // per-candidate features and Achterberg product scores, but
                // only for candidates whose two child LPs were resolved. An
                // iteration-limited child has no certified degradation and
                // must not be mislabeled as a zero-gain branch.
                if (sb_log_file != nullptr) {
                    std::vector<std::size_t> labeled_candidates;
                    std::vector<double> sb_scores;
                    labeled_candidates.reserve(sb_res.candidates.size());
                    sb_scores.reserve(sb_res.candidates.size());
                    for (const auto& c : sb_res.candidates) {
                        if (c.down_resolved && c.up_resolved && std::isfinite(c.score)) {
                            labeled_candidates.push_back(c.variable_index);
                            sb_scores.push_back(c.score);
                        }
                    }
                    if (!labeled_candidates.empty()) {
                        const auto graph = extract_bipartite_features(
                            node_lp_res.primal, current_node_model.variable_type,
                            labeled_candidates, feature_pseudo_costs, current_node_model,
                            node_lp_res.row_dual);
                        ml::append_sb_record(*sb_log_file, graph, sb_scores);
                    }
                }
#endif
            } catch (const std::bad_alloc&) {
                throw;
            } catch (const std::exception& e) {
                if (std::getenv("MARKOV_NODE_DEBUG")) {
                    std::fprintf(stderr, "[milp] node strong branching failed: %s\n", e.what());
                }
                branch_var = select_branching_variable(node_lp_res.primal, root_model.variable_type,
                                                       pseudo_costs, options.branching_strategy,
                                                       options.integrality_tolerance,
                                                       &current_node_model,
                                                       branching_scorer,
                                                       result.ml_requested ? &result.ml_telemetry
                                                                           : nullptr,
                                                       &node_lp_res.row_dual);
            }
        } else {
            branch_var = select_branching_variable(node_lp_res.primal, root_model.variable_type,
                                                   pseudo_costs, options.branching_strategy,
                                                   options.integrality_tolerance,
                                                   &current_node_model,
                                                   branching_scorer,
                                                   result.ml_requested ? &result.ml_telemetry
                                                                       : nullptr,
                                                   &node_lp_res.row_dual);
        }

        if (branch_var >= root_model.matrix.column_count) {
            continue;
        }

        const double branch_val = node_lp_res.primal[branch_var];
        const double floor_val = std::floor(branch_val);
        const double ceil_val = std::ceil(branch_val);

        // Child 1 (Down Branch): x_k <= floor_val
        bool down_valid = true;
        if (node->variable_lower[branch_var].is_finite() &&
            floor_val < node->variable_lower[branch_var].value - 1e-9) {
            down_valid = false;
        }
        if (down_valid) {
            auto down_child = std::make_shared<BranchNode>();
            down_child->id = next_node_id++;
            down_child->parent_id = node->id;
            down_child->depth = node->depth + 1;
            down_child->lower_bound =
                node_lp_res.lower_bound; // conservative dual bound is valid for descendants
            down_child->branch_variable = branch_var;
            down_child->branch_value = branch_val;
            down_child->is_down_branch = true;
            down_child->variable_lower = node->variable_lower;
            down_child->variable_upper = node->variable_upper;
            down_child->variable_upper[branch_var] = model::Bound::finite(floor_val);
            down_child->warm_basis = node_lp_res.basis;
            down_child->local_cuts = node->local_cuts;
            queue.push(down_child);
        }

        // Child 2 (Up Branch): x_k >= ceil_val
        bool up_valid = true;
        if (node->variable_upper[branch_var].is_finite() &&
            ceil_val > node->variable_upper[branch_var].value + 1e-9) {
            up_valid = false;
        }
        if (up_valid) {
            auto up_child = std::make_shared<BranchNode>();
            up_child->id = next_node_id++;
            up_child->parent_id = node->id;
            up_child->depth = node->depth + 1;
            up_child->lower_bound = node_lp_res.lower_bound;
            up_child->branch_variable = branch_var;
            up_child->branch_value = branch_val;
            up_child->is_down_branch = false;
            up_child->variable_lower = node->variable_lower;
            up_child->variable_upper = node->variable_upper;
            up_child->variable_lower[branch_var] = model::Bound::finite(ceil_val);
            up_child->warm_basis = node_lp_res.basis;
            up_child->local_cuts = node->local_cuts;
            queue.push(up_child);
        }

        // Update global lower bound from active queue. The frontier MINIMUM is
        // required: queue.top() is only that under best_bound ordering (see
        // NodeFrontier).
        if (!queue.empty()) {
            best_lower_bound = std::min(best_upper_bound, queue.min_lower_bound());
        }

        // Check relative optimality gap
        if (!best_primal.empty() && best_lower_bound > -std::numeric_limits<double>::infinity()) {
            const double gap = std::abs(best_upper_bound - best_lower_bound) /
                               std::max(1.0, std::abs(best_upper_bound));
            if (gap <= options.relative_gap_tolerance) {
                break;
            }
        }
    }

    // 7. Assemble Final Result
    const auto end_time = std::chrono::steady_clock::now();
    result.runtime_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    bool gap_closed = false;
    if (best_lower_bound > -std::numeric_limits<double>::infinity()) {
        const double gap = std::abs(best_upper_bound - best_lower_bound) /
                           std::max(1.0, std::abs(best_upper_bound));
        gap_closed = gap <= options.relative_gap_tolerance;
    }
    // R13/R17: optimality may only be claimed when the tree was exhausted with
    // every node relaxation certified. An uncertified node LP leaves its bound
    // unknown, so the proof is incomplete by construction.
    const bool proven = (queue.empty() || gap_closed) && unsolved_node_lps == 0;

    if (!best_primal.empty()) {
        result.primal = std::move(best_primal);
        result.objective = best_upper_bound;
        result.best_bound = (queue.empty() && unsolved_node_lps == 0)
                                ? best_upper_bound
                                : std::min(best_lower_bound, min_unsolved_bound);
        if (std::abs(result.best_bound) > 1e15) result.best_bound = result.objective;
        result.relative_gap = std::max(0.0, std::abs(result.objective - result.best_bound) /
                                                std::max(1.0, std::abs(result.objective)));
        if (proven) {
            result.status = lp::reference::SolveStatus::optimal;
            result.message = "branch-and-cut MILP optimum";
        } else {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message =
                unsolved_node_lps > 0
                    ? ("search incomplete: " + std::to_string(unsolved_node_lps) +
                       " node LP(s) could not be certified; optimality not proven")
                    : (stop_reason.empty() ? "search stopped before proof of optimality"
                                           : stop_reason + " before proof of optimality");
        }
    } else if (queue.empty() && unsolved_node_lps == 0) {
        result.status = lp::reference::SolveStatus::infeasible;
        result.message = "no integer feasible solution found";
    } else {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.best_bound = std::min(best_lower_bound, min_unsolved_bound);
        result.message = unsolved_node_lps > 0
                             ? "search incomplete: node LP could not be certified; infeasibility not proven"
                             : (stop_reason.empty() ? "search stopped with no incumbent"
                                                    : stop_reason + " with no incumbent");
    }
    if (result.ml_requested && result.ml_model_loaded) {
        if (!result.ml_telemetry.fallback_reason.empty()) {
            result.ml_fallback_reason = result.ml_telemetry.fallback_reason;
        } else if (result.ml_telemetry.scored_nodes > 0) {
            result.ml_fallback_reason.clear();
        } else if (result.ml_telemetry.eligible_nodes > 0) {
            result.ml_fallback_reason = "eligible_nodes_not_scored";
        } else if (result.ml_telemetry.maximum_candidate_count > 0) {
            result.ml_fallback_reason = "fractional_candidate_gate_not_met";
        } else {
            result.ml_fallback_reason = "no_fractional_branch_node_observed";
        }
    }
    return result;
}

} // namespace markov_cero::milp
