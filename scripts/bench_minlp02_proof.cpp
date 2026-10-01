// MINLP-02 (docs/contracts/minlp-proof-replay.md §8): independent OA proof
// benchmark over the frozen four-case stratum of minlp-oa.md §11. Each case
// runs through api::solve_model twice — shared proof budgets on (default)
// and enable_mip_proof = false — and records status, assurance, proof
// status/tier, build and replay milliseconds, serialized proof bytes
// (write_oa_proof) and objective/bound/gap. The summary reports the
// accepted fraction (expected 4/4), the total bytes and the disclosed
// on/off wall difference — disclosure only, never a speed claim.
//
//   minlp02_proof_benchmark [--out <evidence.json>]
#include "markov_cero/api/solve.hpp"
#include "markov_cero/verify/oa_proof.hpp"

#include <chrono>
#include <cmath>
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
        if (c == '\n') { out += "\\n"; continue; }
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

// Frozen minlp-oa.md section 11 stratum (identical models to
// scripts/bench_minlp01_oa.cpp so the two records stay comparable).
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

// Source optimum 0.75; api::solve_model reports the source sense.
model::Model maximize_source() {
    model::Model source = basic_source();
    source.name = "bench_maximize_offset";
    source.objective_sense = model::ObjectiveSense::maximize;
    source.nlobj_terms = {{-1.0, 0, 0, true}, {-1.0, 1, 1, true}};
    source.objective_offset = 2.0;
    source.validate();
    return source;
}

std::size_t proof_bytes(const api::SolveResult& res) {
    if (!res.oa_proof) return 0;
    std::ostringstream out;
    verify::write_oa_proof(out, *res.oa_proof);
    return out.str().size();
}

// One timed solve after an untimed warm-up of the same shape.
api::SolveResult timed_solve(const model::Model& source, bool enable_proof,
                             double* wall_ms) {
    api::SolveOptions warm;
    warm.enable_mip_proof = enable_proof;
    (void)api::solve_model(source, warm);
    api::SolveOptions options;
    options.enable_mip_proof = enable_proof;
    const auto start = Clock::now();
    const api::SolveResult res = api::solve_model(source, options);
    *wall_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return res;
}

struct CaseRecord {
    std::string json;
    bool accepted = false;
    std::size_t bytes = 0;
    double on_wall_ms = 0.0;
    double off_wall_ms = 0.0;
};

CaseRecord run_case(const std::string& name, const model::Model& source,
                    double known_optimum, const std::string& source_sense) {
    double on_ms = 0.0;
    double off_ms = 0.0;
    const api::SolveResult on = timed_solve(source, true, &on_ms);
    const api::SolveResult off = timed_solve(source, false, &off_ms);
    const std::size_t bytes = proof_bytes(on);

    std::cout << "  " << name << ": " << status_name(on.status)
              << " status=" << on.assurance << "/" << on.guarantee_tier << "/"
              << on.proof_status << " obj=" << json_number(on.objective)
              << " bound=" << json_number(on.best_bound) << " gap="
              << json_number(on.relative_gap) << " bytes=" << bytes << "\n";

    CaseRecord record;
    record.accepted = on.proof_status == "accepted";
    record.bytes = bytes;
    record.on_wall_ms = on_ms;
    record.off_wall_ms = off_ms;
    std::ostringstream out;
    out << "    {\n"
        << "      \"case\": \"" << json_escape(name) << "\",\n"
        << "      \"source_sense\": \"" << source_sense << "\",\n"
        << "      \"known_optimum\": " << json_number(known_optimum) << ",\n"
        << "      \"proof_on\": {\n"
        << "        \"status\": \"" << status_name(on.status) << "\",\n"
        << "        \"assurance\": \"" << json_escape(on.assurance) << "\",\n"
        << "        \"guarantee_tier\": \"" << json_escape(on.guarantee_tier) << "\",\n"
        << "        \"proof_status\": \"" << json_escape(on.proof_status) << "\",\n"
        << "        \"canonical_verified\": " << (on.canonical_verified ? "true" : "false")
        << ",\n"
        << "        \"build_ms\": " << json_number(on.oa_proof_build_ms) << ",\n"
        << "        \"replay_ms\": " << json_number(on.oa_proof_verify_ms) << ",\n"
        << "        \"proof_bytes\": " << bytes << ",\n"
        << "        \"objective\": " << json_number(on.objective) << ",\n"
        << "        \"best_bound\": " << json_number(on.best_bound) << ",\n"
        << "        \"relative_gap\": " << json_number(on.relative_gap) << ",\n"
        << "        \"wall_ms\": " << json_number(on_ms) << "\n"
        << "      },\n"
        << "      \"proof_off\": {\n"
        << "        \"status\": \"" << status_name(off.status) << "\",\n"
        << "        \"assurance\": \"" << json_escape(off.assurance) << "\",\n"
        << "        \"proof_status\": \"" << json_escape(off.proof_status) << "\",\n"
        << "        \"objective\": " << json_number(off.objective) << ",\n"
        << "        \"best_bound\": " << json_number(off.best_bound) << ",\n"
        << "        \"relative_gap\": " << json_number(off.relative_gap) << ",\n"
        << "        \"wall_ms\": " << json_number(off_ms) << "\n"
        << "      }\n"
        << "    }";
    record.json = out.str();
    return record;
}
} // namespace

int main(int argc, char** argv) {
    std::string out_path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else {
            std::cerr << "usage: minlp02_proof_benchmark [--out <evidence.json>]\n";
            return 2;
        }
    }
    try {
        std::vector<std::string> records;
        std::size_t accepted = 0;
        std::size_t total_bytes = 0;
        double on_total = 0.0;
        double off_total = 0.0;
        const auto add = [&](const std::string& name, const model::Model& source,
                             double known, const std::string& sense) {
            CaseRecord record = run_case(name, source, known, sense);
            if (record.accepted) ++accepted;
            total_bytes += record.bytes;
            on_total += record.on_wall_ms;
            off_total += record.off_wall_ms;
            records.push_back(std::move(record.json));
        };
        add("basic_quadratic", basic_source(), 1.25, "minimize");
        add("case_a_binary", case_a_source(), 0.2, "minimize");
        add("two_integer_oracle", oracle_source(), 0.3, "minimize");
        add("maximize_offset", maximize_source(), 0.75, "maximize");

        std::ostringstream json;
        json << "{\n"
             << "  \"record\": \"minlp02-proof\",\n"
             << "  \"date\": \"2026-10-01\",\n"
             << "  \"contract\": \"docs/contracts/minlp-proof-replay.md section 8\",\n"
             << "  \"entry_point\": \"library: api::solve_model\",\n"
             << "  \"design\": \"Frozen minlp-oa.md section 11 four-case stratum; each "
                "case solved twice through api::solve_model with shared proof budgets "
                "on (default) and enable_mip_proof = false; warm-up solve before each "
                "timed solve; proof bytes measured with write_oa_proof.\",\n"
             << "  \"cases\": [\n";
        for (std::size_t k = 0; k < records.size(); ++k) {
            json << records[k] << (k + 1 < records.size() ? ",\n" : "\n");
        }
        json << "  ],\n"
             << "  \"summary\": {\n"
             << "    \"cases\": " << records.size() << ",\n"
             << "    \"accepted\": " << accepted << ",\n"
             << "    \"accepted_fraction\": \"" << accepted << "/" << records.size()
             << "\",\n"
             << "    \"total_proof_bytes\": " << total_bytes << ",\n"
             << "    \"wall_ms_proof_on\": " << json_number(on_total) << ",\n"
             << "    \"wall_ms_proof_off\": " << json_number(off_total) << ",\n"
             << "    \"wall_difference_ms\": " << json_number(on_total - off_total)
             << ",\n"
             << "    \"note\": \"Wall difference is disclosure only: one machine, one "
                "run, no speed claim. Accepted fraction is the acceptance gate "
                "(minlp-proof-replay.md section 8).\"\n"
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
