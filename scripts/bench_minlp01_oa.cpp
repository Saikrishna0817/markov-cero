// MINLP-01 (docs/contracts/minlp-oa.md §11): separate restricted convex
// quadratic MINLP benchmark stratum. Embedded analytic cases with known
// optima, each run through minlp::solve_minlp directly because the required
// counters (sqp_calls, sqp_failures, master_nodes, cuts_replayed) live on
// MinlpSolution, not SolveResult.
//
//   minlp01_oa_benchmark [--out <evidence.json>]
//
// Statuses are recorded as measured; no speed claim, no claim beyond the
// solver-trusted assurance level of minlp-oa.md section 1.
#include "markov_cero/minlp/minlp_solver.hpp"
#include "markov_cero/io/nlobj_parser.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace markov_cero;
using Clock = std::chrono::steady_clock;

std::string status_name(lp::reference::SolveStatus status) {
    switch (status) {
    case lp::reference::SolveStatus::optimal: return "Optimal";
    case lp::reference::SolveStatus::infeasible: return "Infeasible";
    case lp::reference::SolveStatus::unbounded: return "Unbounded";
    case lp::reference::SolveStatus::iteration_limit: return "IterationLimit";
    case lp::reference::SolveStatus::invalid_model: return "InvalidModel";
    case lp::reference::SolveStatus::invalid_options: return "InvalidOptions";
    case lp::reference::SolveStatus::resource_limit: return "ResourceLimit";
    case lp::reference::SolveStatus::numerical_failure: return "NumericalFailure";
    case lp::reference::SolveStatus::unsupported: return "Unsupported";
    case lp::reference::SolveStatus::non_convex_minlp: return "NonConvexMINLP";
    case lp::reference::SolveStatus::gap_satisfied: return "GapSatisfied";
    case lp::reference::SolveStatus::local_optimal: return "LocalStationary";
    case lp::reference::SolveStatus::feasible: return "Feasible";
    }
    return "Unknown";
}

std::string json_escape(const std::string& in) {
    std::string out;
    for (char c : in) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out;
}

std::string json_number(double v) {
    if (!std::isfinite(v)) return "null";
    std::ostringstream stream;
    stream << std::setprecision(17) << v;
    return stream.str();
}

model::Model basic_source() {
    model::Model source;
    source.name = "bench_basic";
    model::SparseMatrixBuilder rows(1, 2);
    rows.add(0, 0, 1.0);
    rows.add(0, 1, 1.0);
    source.matrix = rows.build();
    source.objective = {0.0, 0.0};
    source.row_name = {"demand"};
    source.row_lower = {model::Bound::finite(1.5)};
    source.row_upper = {model::Bound::positive_infinity()};
    source.variable_name = {"x0", "x1"};
    source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(3.0), model::Bound::finite(3.0)};
    source.variable_type = {model::VariableType::continuous, model::VariableType::integer};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{1.0, 0, 0, true}, {1.0, 1, 1, true}};
    source.validate();
    return source;
}

// min (x-1)^2 + 0.2y with x^2 <= y (minlp-oa.md case A); optimum 0.2.
model::Model case_a_source() {
    model::Model source;
    source.name = "bench_case_a";
    source.matrix = model::SparseMatrixBuilder(0, 2).build();
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

// min (x-1)^2 + 0.3y1 + 0.5y2 with x^2 <= y1+y2; optimum 0.3 (enumerated).
model::Model oracle_source() {
    model::Model source;
    source.name = "bench_oracle2";
    source.matrix = model::SparseMatrixBuilder(0, 3).build();
    source.objective = {-2.0, 0.3, 0.5};
    source.objective_offset = 1.0;
    source.variable_name = {"x", "y1", "y2"};
    source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0),
                             model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(2.0), model::Bound::finite(1.0),
                             model::Bound::finite(1.0)};
    source.variable_type = {model::VariableType::continuous, model::VariableType::integer,
                            model::VariableType::integer};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{1.0, 0, 0, true}};
    source.nlcon_constraints = {
        {"x2_le_y12", 0.0, {{1.0, 0, 0, true}, {-1.0, 1, 0, false}, {-1.0, 2, 0, false}}}};
    source.validate();
    return source;
}

// Maximize -(x0^2+x1^2)+2 with x0+x1 >= 1.5, x1 integer. Source optimum
// 0.75; the benchmark records the normalized internal minimization sense
// (-0.75) because MinlpSolution reports internal values (minlp-oa.md §2).
model::Model maximize_source() {
    model::Model source = basic_source();
    source.name = "bench_maximize_offset";
    source.objective_sense = model::ObjectiveSense::maximize;
    source.nlobj_terms = {{-1.0, 0, 0, true}, {-1.0, 1, 1, true}};
    source.objective_offset = 2.0;
    source.validate();
    return source;
}

std::string run_case(const std::string& name, const model::Model& source,
                     const std::vector<std::size_t>& integers,
                     const std::vector<double>& x0, double internal_optimum,
                     const std::string& source_sense) {
    minlp::MinlpProblem problem;
    problem.source_model = source;
    problem.nlp = io::make_nlp_model(source);
    problem.integer_indices = integers;
    const minlp::MinlpOptions options;
    (void)minlp::solve_minlp(problem, x0, options); // warm-up, untimed
    const auto start = Clock::now();
    const minlp::MinlpSolution sol = minlp::solve_minlp(problem, x0, options);
    const double wall_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();

    const std::string line =
        "  " + name + ": " + status_name(sol.status) +
        " obj=" + json_number(sol.objective) + " bound=" + json_number(sol.best_bound) +
        " gap=" + json_number(sol.relative_gap) + " iters=" +
        std::to_string(sol.iterations) + " sqp=" + std::to_string(sol.sqp_calls) +
        " sqp_fail=" + std::to_string(sol.sqp_failures) + " cuts=" +
        std::to_string(sol.cuts_added) + " nodes=" + std::to_string(sol.master_nodes) +
        " replayed=" + std::to_string(sol.cuts_replayed) + " wall_ms=" +
        std::to_string(wall_ms);
    std::cout << line << "\n";

    std::ostringstream out;
    out << "    {\n"
        << "      \"case\": \"" << json_escape(name) << "\",\n"
        << "      \"source_sense\": \"" << source_sense << "\",\n"
        << "      \"known_internal_optimum\": " << json_number(internal_optimum) << ",\n"
        << "      \"status\": \"" << status_name(sol.status) << "\",\n"
        << "      \"message\": \"" << json_escape(sol.message) << "\",\n"
        << "      \"integer_feasible\": " << (sol.integer_feasible ? "true" : "false") << ",\n"
        << "      \"objective\": " << json_number(sol.objective) << ",\n"
        << "      \"best_bound\": " << json_number(sol.best_bound) << ",\n"
        << "      \"bound_provenance\": \"" << json_escape(sol.bound_provenance) << "\",\n"
        << "      \"relative_gap\": " << json_number(sol.relative_gap) << ",\n"
        << "      \"iterations\": " << sol.iterations << ",\n"
        << "      \"sqp_calls\": " << sol.sqp_calls << ",\n"
        << "      \"sqp_failures\": " << sol.sqp_failures << ",\n"
        << "      \"cuts_added\": " << sol.cuts_added << ",\n"
        << "      \"master_nodes\": " << sol.master_nodes << ",\n"
        << "      \"cuts_replayed\": " << sol.cuts_replayed << ",\n"
        << "      \"wall_ms\": " << json_number(wall_ms) << "\n"
        << "    }";
    return out.str();
}
} // namespace

int main(int argc, char** argv) {
    std::string out_path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else {
            std::cerr << "usage: minlp01_oa_benchmark [--out <evidence.json>]\n";
            return 2;
        }
    }
    try {
        const model::Model basic = basic_source();
        const model::Model case_a = case_a_source();
        const model::Model oracle = oracle_source();
        const model::Model maximize = maximize_source();
        std::vector<std::string> records;
        records.push_back(
            run_case("basic_quadratic", basic, {1}, {0.2, 0.2}, 1.25, "minimize"));
        records.push_back(run_case("case_a_binary", case_a, {1}, {0.0, 0.0}, 0.2, "minimize"));
        records.push_back(
            run_case("two_integer_oracle", oracle, {1, 2}, {0.5, 0.0, 0.0}, 0.3, "minimize"));
        records.push_back(
            run_case("maximize_offset", maximize, {1}, {0.2, 0.2}, -0.75, "maximize"));

        std::ostringstream json;
        json << "{\n"
             << "  \"record\": \"minlp01-oa-stratum\",\n"
             << "  \"date\": \"2026-10-01\",\n"
             << "  \"contract\": \"docs/contracts/minlp-oa.md section 11\",\n"
             << "  \"entry_point\": \"library: minlp::solve_minlp\",\n"
             << "  \"design\": \"Embedded restricted convex quadratic MINLP cases with known "
                "optima; warm-up solve before each timed solve; statuses and counters as "
                "measured; maximize case recorded in the normalized internal sense.\",\n"
             << "  \"cases\": [\n";
        for (std::size_t k = 0; k < records.size(); ++k) {
            json << records[k] << (k + 1 < records.size() ? ",\n" : "\n");
        }
        json << "  ],\n"
             << "  \"summary\": {\n"
             << "    \"cases\": " << records.size() << ",\n"
             << "    \"note\": \"No speed claim; solver-trusted global bounds only "
                "(minlp-oa.md section 1).\"\n"
             << "  }\n"
             << "}\n";

        if (!out_path.empty()) {
            std::ofstream file(out_path);
            if (!file) {
                std::cerr << "FAIL: cannot write " << out_path << "\n";
                return 1;
            }
            file << json.str();
            std::cout << "wrote " << out_path << "\n";
        } else {
            std::cout << json.str();
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
