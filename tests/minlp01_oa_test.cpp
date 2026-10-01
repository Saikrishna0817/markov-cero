// MINLP-01 (docs/contracts/minlp-oa.md §10): analytic case A with tangent
// validity, exhaustive small-integer oracles, SQP-failure honesty (case F),
// master node limit (case G), the OA row cap and the paired counters.
#include "markov_cero/minlp/minlp_solver.hpp"
#include "markov_cero/io/nlobj_parser.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    double out = 0.0;
    for (std::size_t j = 0; j < a.size(); ++j) out += a[j] * b[j];
    return out;
}

// Case A: x in [0,2], y in {0,1}, min (x-1)^2 + 0.2y, x^2 <= y.
model::Model case_a_source() {
    model::Model source;
    source.name = "minlp01_case_a";
    source.matrix = model::SparseMatrixBuilder(0, 2).build(); // rows come from NLCON
    source.objective = {-2.0, 0.2};
    source.objective_offset = 1.0;
    source.variable_name = {"x", "y"};
    source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(2.0), model::Bound::finite(1.0)};
    source.variable_type = {model::VariableType::continuous, model::VariableType::integer};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{1.0, 0, 0, true}};
    source.nlcon_constraints = {
        {"x2_le_y", 0.0, {{1.0, 0, 0, true}, {-1.0, 1, 0, false}}}};
    source.validate();
    return source;
}

minlp::MinlpProblem case_a_problem(const model::Model& source) {
    minlp::MinlpProblem problem;
    problem.source_model = source;
    problem.nlp = io::make_nlp_model(source);
    problem.integer_indices = {1};
    return problem;
}

// Contract §10 case A: a stored tangent never excludes a feasible original
// point and never under-estimates the source function by more than noise.
void require_tangent_valid(const model::Model& source, const minlp::OaCut& cut,
                           const std::vector<std::vector<double>>& feasible) {
    for (const auto& at : feasible) {
        const double row_value = dot(cut.gradient, at) - cut.rhs;
        const double actual =
            minlp::derive_oa_cut(source, cut.source_kind, cut.source_index, at).value;
        const std::string message =
            "tangent excludes or under-estimates a feasible point (row " +
            std::to_string(row_value) + " vs actual " + std::to_string(actual) + ")";
        req(row_value <= actual + 1e-6, message.c_str());
    }
}

} // namespace

int main() {
    minlp::MinlpOptions opts;

    // ---- Case A: analytic optimum, tangent validity, paired counters. ----
    const model::Model source = case_a_source();
    const minlp::MinlpProblem problem = case_a_problem(source);
    const auto sol = minlp::solve_minlp(problem, {0.0, 0.0}, opts);
    req(sol.status == lp::reference::SolveStatus::optimal, "case A reaches optimal");
    req(sol.integer_feasible && sol.x.size() == 2, "case A returns an incumbent");
    req(std::abs(sol.objective - 0.2) <= 1e-6, "case A objective is exactly 0.2");
    req(std::abs(sol.x[0] - 1.0) <= 1e-3 && std::abs(sol.x[1] - 1.0) <= 1e-3,
        "case A incumbent is near (1,1)");
    req(std::isfinite(sol.best_bound) && sol.best_bound <= sol.objective + 1e-6,
        "case A bound is finite and never exceeds the incumbent");
    req(sol.relative_gap <= opts.gap_tolerance, "case A gap satisfies the locked tolerance");
    req(sol.bound_provenance == "milp_master_certified",
        "case A bound carries master assurance provenance");
    // Counters (contract §8): one SQP call per iteration, every stored cut
    // replayed, master tree growth recorded.
    req(sol.sqp_calls == sol.iterations, "sqp_calls counts one subproblem per iteration");
    req(sol.master_nodes >= 1, "master_nodes records the master tree");
    req(sol.cuts_added >= 2 && sol.cuts_added == sol.oa_cuts.size(),
        "every master row has a provenance record");
    req(sol.cuts_replayed == sol.cuts_added, "every stored cut was source-replayed");
    // The master ties y=0 against y=1 at the first relaxation bound, so the
    // OA loop pins the degenerate subproblem x^2 <= 0 once; that subproblem
    // may legitimately fail hessian resets, and the loop must survive it
    // with the optimal status and verified incumbent asserted above.
    req(sol.sqp_failures <= 1, "healthy case A survives at most the degenerate-pin failure");

    // Tangents at x = 0, 0.5, 1 (contract case A) from direct derivation.
    const std::vector<std::vector<double>> feasible = {
        {0.0, 0.0}, {0.0, 1.0}, {0.25, 1.0}, {0.5, 1.0}, {0.75, 1.0}, {1.0, 1.0}};
    for (double px : {0.0, 0.5, 1.0}) {
        const std::vector<double> point = {px, 1.0};
        const auto obj_cut = minlp::derive_oa_cut(
            source, minlp::OaCutSource::objective, minlp::kOaObjectiveSource, point);
        const auto nlcon_cut = minlp::derive_oa_cut(source, minlp::OaCutSource::nlcon, 0, point);
        require_tangent_valid(source, obj_cut, feasible);
        require_tangent_valid(source, nlcon_cut, feasible);
    }
    // The cuts produced by the solve itself must pass the same check.
    for (const auto& cut : sol.oa_cuts) require_tangent_valid(source, cut, feasible);

    // ---- Exhaustive small-integer oracle (contract §10). ----
    // y=0 -> x=0, f=1; y=1 -> x in [0,1], min at x=1 gives 0.2; min = 0.2.
    const double oracle_a = 1.0;
    const double oracle_a_y1 = 0.2;
    req(std::min(oracle_a, oracle_a_y1) == 0.2, "analytic enumeration of case A");
    req(std::abs(sol.objective - std::min(oracle_a, oracle_a_y1)) <= 1e-6,
        "OA incumbent matches the exhaustive enumeration minimum");

    // Second oracle: min (x-1)^2 + 0.3y1 + 0.5y2, x^2 <= y1+y2, binaries.
    // (y1,y2) in {(0,0),(1,0),(0,1),(1,1)} -> {1.0, 0.3, 0.5, 0.8}; min = 0.3.
    model::Model oracle_source;
    oracle_source.name = "minlp01_oracle2";
    oracle_source.matrix = model::SparseMatrixBuilder(0, 3).build();
    oracle_source.objective = {-2.0, 0.3, 0.5};
    oracle_source.objective_offset = 1.0;
    oracle_source.variable_name = {"x", "y1", "y2"};
    oracle_source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0),
                                     model::Bound::finite(0.0)};
    oracle_source.variable_upper = {model::Bound::finite(2.0), model::Bound::finite(1.0),
                                     model::Bound::finite(1.0)};
    oracle_source.variable_type = {model::VariableType::continuous, model::VariableType::integer,
                                    model::VariableType::integer};
    oracle_source.has_nlobj_section = true;
    oracle_source.nlobj_terms = {{1.0, 0, 0, true}};
    oracle_source.nlcon_constraints = {
        {"x2_le_y12", 0.0, {{1.0, 0, 0, true}, {-1.0, 1, 0, false}, {-1.0, 2, 0, false}}}};
    oracle_source.validate();
    minlp::MinlpProblem oracle_problem;
    oracle_problem.source_model = oracle_source;
    oracle_problem.nlp = io::make_nlp_model(oracle_source);
    oracle_problem.integer_indices = {1, 2};
    const double oracle_values[4] = {1.0, 0.3, 0.5, 0.8};
    double oracle_min = oracle_values[0];
    for (double v : oracle_values) oracle_min = std::min(oracle_min, v);
    const auto oracle_sol = minlp::solve_minlp(oracle_problem, {0.5, 0.0, 0.0}, opts);
    req(oracle_sol.status == lp::reference::SolveStatus::optimal,
        "two-integer oracle model reaches optimal");
    req(std::abs(oracle_sol.objective - oracle_min) <= 1e-6,
        "two-integer OA result matches the exhaustive enumeration minimum");
    req(std::isfinite(oracle_sol.best_bound) && oracle_sol.best_bound <= oracle_min + 1e-6,
        "oracle-model bound is valid against the enumeration");

    // ---- Case F: SQP failure honesty (infeasible-pair shape). ----
    model::Model fail_source;
    fail_source.name = "minlp01_case_f";
    model::SparseMatrixBuilder fail_rows(2, 1);
    fail_rows.add(0, 0, 1.0);
    fail_rows.add(1, 0, 1.0);
    fail_source.matrix = fail_rows.build();
    fail_source.objective = {0.0};
    fail_source.row_name = {"le_zero", "ge_one"};
    fail_source.row_lower = {model::Bound::negative_infinity(), model::Bound::finite(1.0)};
    fail_source.row_upper = {model::Bound::finite(0.0), model::Bound::positive_infinity()};
    fail_source.variable_name = {"x"};
    fail_source.variable_lower = {model::Bound::finite(-5.0)};
    fail_source.variable_upper = {model::Bound::finite(5.0)};
    fail_source.variable_type = {model::VariableType::continuous};
    fail_source.has_nlobj_section = true;
    fail_source.nlobj_terms = {{1.0, 0, 0, true}};
    fail_source.validate();
    minlp::MinlpProblem fail_problem;
    fail_problem.source_model = fail_source;
    fail_problem.nlp = io::make_nlp_model(fail_source);
    const auto fail_sol = minlp::solve_minlp(fail_problem, {2.0}, opts);
    req(fail_sol.status == lp::reference::SolveStatus::infeasible,
        "case F stops honestly on the master relaxation");
    req(fail_sol.sqp_calls >= 1 && fail_sol.sqp_failures >= 1,
        "case F counts the failed subproblem");
    req(!fail_sol.integer_feasible && fail_sol.x.empty(),
        "case F accepts no incumbent from a failed SQP");
    req(fail_sol.oa_cuts.size() >= 3 && fail_sol.cuts_replayed == fail_sol.cuts_added,
        "case F keeps its cut records and replays them");
    req(!std::isfinite(fail_sol.best_bound), "case F reports no bound without a master bound");

    // ---- Case G: master node limit keeps claims honest. ----
    model::Model integral_source;
    integral_source.name = "minlp01_case_g";
    integral_source.matrix = model::SparseMatrixBuilder(0, 2).build();
    integral_source.objective = {-2.0, -2.0};
    integral_source.objective_offset = 2.0;
    integral_source.variable_name = {"x", "y"};
    integral_source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    integral_source.variable_upper = {model::Bound::finite(3.0), model::Bound::finite(3.0)};
    integral_source.variable_type = {model::VariableType::continuous, model::VariableType::integer};
    integral_source.has_nlobj_section = true;
    integral_source.nlobj_terms = {{1.0, 0, 0, true}, {1.0, 1, 1, true}};
    integral_source.validate();
    minlp::MinlpProblem integral_problem;
    integral_problem.source_model = integral_source;
    integral_problem.nlp = io::make_nlp_model(integral_source);
    integral_problem.integer_indices = {1};
    minlp::MinlpOptions one_node = opts;
    one_node.milp_max_nodes = 1;
    const auto g1 = minlp::solve_minlp(integral_problem, {2.5, 2.5}, one_node);
    req(g1.integer_feasible && g1.x.size() == 2,
        "case G keeps the verified incumbent under a node-limited master");
    req(g1.status == lp::reference::SolveStatus::optimal ||
            g1.status == lp::reference::SolveStatus::feasible ||
            g1.status == lp::reference::SolveStatus::iteration_limit ||
            g1.status == lp::reference::SolveStatus::resource_limit,
        "case G reports only statuses the node-limited master can justify");
    if (g1.status == lp::reference::SolveStatus::optimal) {
        req(std::isfinite(g1.relative_gap) && g1.relative_gap <= opts.gap_tolerance,
            "case G optimal only with the locked gap closed");
    }
    if (std::isfinite(g1.best_bound)) {
        req(g1.best_bound <= g1.objective + 1e-6, "case G bound never exceeds the incumbent");
        req(g1.bound_provenance == "milp_master_certified",
            "case G finite bound carries provenance");
    } else {
        req(g1.bound_provenance.empty(), "case G never claims provenance without a bound");
    }
    req(g1.master_nodes <= 1, "case G respects milp_max_nodes");

    // Case G2: the gap-closable case A model under the same node limit must
    // still never fabricate optimality or an unverified incumbent.
    minlp::MinlpOptions a_one_node = opts;
    a_one_node.milp_max_nodes = 1;
    const auto g2 = minlp::solve_minlp(case_a_problem(source), {0.0, 0.0}, a_one_node);
    if (g2.status == lp::reference::SolveStatus::optimal) {
        req(std::isfinite(g2.relative_gap) && g2.relative_gap <= opts.gap_tolerance,
            "case G2 optimal only when the gap actually closed");
    }
    if (g2.integer_feasible) {
        req(std::isfinite(g2.best_bound) && g2.best_bound <= g2.objective + 1e-6,
            "case G2 retained incumbent carries a certified valid bound");
    } else {
        req(g2.x.empty(), "case G2 has no incumbent fields without an incumbent");
    }
    if (std::isfinite(g2.best_bound)) {
        req(g2.best_bound <= 0.184, "case G2 master bound never exceeds the relaxation value");
        req(g2.bound_provenance == "milp_master_certified", "case G2 bound provenance");
    } else {
        req(g2.bound_provenance.empty(), "case G2 has no provenance without a bound");
    }
    req(g2.master_nodes <= g2.iterations, "case G2 counts at most one node per master solve");

    // ---- OA row cap and option validation (contract §8.2). ----
    minlp::MinlpOptions capped = opts;
    capped.max_oa_cuts = 2;
    const auto cap_sol = minlp::solve_minlp(case_a_problem(source), {0.0, 0.0}, capped);
    req(cap_sol.status == lp::reference::SolveStatus::iteration_limit,
        "OA row cap stops like the iteration limit");
    req(cap_sol.message.find("OA row cap") != std::string::npos,
        "OA row cap stop names the cap");
    req(cap_sol.cuts_added == 2 && cap_sol.oa_cuts.size() == 2,
        "OA row cap never stores more cuts than allowed");
    req(cap_sol.cuts_replayed == cap_sol.cuts_added, "cap-stopped cuts were replayed");

    minlp::MinlpOptions zero_cap = opts;
    zero_cap.max_oa_cuts = 0;
    const auto zero_sol = minlp::solve_minlp(case_a_problem(source), {0.0, 0.0}, zero_cap);
    req(zero_sol.status == lp::reference::SolveStatus::invalid_options,
        "max_oa_cuts=0 is invalid_options");

    std::cout << "minlp01 OA tests passed\n";
    return 0;
}
