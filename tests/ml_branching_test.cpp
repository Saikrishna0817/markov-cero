// W2 gate: ML branching scorer produces valid rankings and the activation
// contract holds (LOCKED): without a compiled scorer / model file, ml_gnn
// requests fall back to pseudo_cost silently and solves still succeed.
//
// Compiled with the ML flag when MARKOV_CERO_ENABLE_ML=ON; always compiled
// as a fallback-contract test otherwise. Set MARKOV_CERO_TEST_ONNX to a
// standard exported GCN model to test C++/ONNX Runtime score parity.

#include "markov_cero/milp/branch_selector.hpp"
#include "markov_cero/milp/milp_solver.hpp"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifdef MARKOV_CERO_ENABLE_ML
#include "markov_cero/milp/ml_branching/onnx_scorer.hpp"
#endif

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

model::Model make_milp(std::size_t n_int, std::size_t seed) {
    // n_int integer variables x_j in [0, 10], one knapsack-like row.
    model::Model m;
    m.name = "ml_branching_fixture";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective.assign(n_int, -1.0);
    model::SparseMatrixBuilder builder(1, n_int);
    for (std::size_t j = 0; j < n_int; ++j) {
        builder.add(0, j, 1.0 + 0.1 * static_cast<double>((j * 7 + seed) % 13));
    }
    m.matrix = builder.build();
    m.row_lower.push_back(model::Bound::negative_infinity());
    m.row_upper.push_back(model::Bound::finite(static_cast<double>(n_int) / 2.0));
    m.variable_lower.assign(n_int, model::Bound::finite(0.0));
    m.variable_upper.assign(n_int, model::Bound::finite(10.0));
    m.variable_type.assign(n_int, model::VariableType::integer);
    m.row_name.push_back("cap");
    m.variable_name.resize(n_int);
    for (std::size_t j = 0; j < n_int; ++j) {
        m.variable_name[j] = "x" + std::to_string(j);
    }
    return m;
}

class CountingScorer final : public milp::IBranchingScorer {
  public:
    mutable std::size_t calls{0};
    std::vector<double> score_candidates(
        const std::vector<milp::NodeFeatureVector>& features) const override {
        ++calls;
        return std::vector<double>(features.size(), 0.0);
    }
};

} // namespace

int main() {
    namespace fs = std::filesystem;

    // 1) Without a per-solve scorer, ml_gnn silently behaves like
    //    pseudo_cost (LOCKED activation contract) and stays deterministic.
    {
        const auto model = make_milp(24, 1);
        std::vector<double> primal(24, 0.5);
        std::vector<milp::VariablePseudoCost> pc(24);
        const auto a = milp::select_branching_variable(
            primal, model.variable_type, pc, milp::BranchingStrategy::ml_gnn);
        const auto b = milp::select_branching_variable(
            primal, model.variable_type, pc, milp::BranchingStrategy::pseudo_cost);
        req(a == b, "ml_gnn without scorer matches pseudo_cost");
    }

    // Graph extraction includes active-row features and candidate-to-row
    // edges, so the GCN input is a real bipartite graph rather than a per-node
    // MLP feature matrix.
    {
        model::Model graph_model;
        model::SparseMatrixBuilder builder(1, 2);
        builder.add(0, 0, 1.0);
        builder.add(0, 1, 1.0);
        graph_model.matrix = builder.build();
        graph_model.objective = {1.0, 2.0};
        graph_model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
        graph_model.variable_upper = {model::Bound::finite(1.0), model::Bound::finite(1.0)};
        graph_model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
        graph_model.variable_upper = {model::Bound::finite(1.0), model::Bound::finite(1.0)};
        graph_model.variable_type = {model::VariableType::integer, model::VariableType::integer};
        graph_model.row_lower = {model::Bound::negative_infinity()};
        graph_model.row_upper = {model::Bound::finite(1.0)};
        std::vector<milp::VariablePseudoCost> pseudo_costs(2);
        const auto graph = milp::extract_bipartite_features(
            {0.5, 0.5}, graph_model.variable_type, {0, 1}, pseudo_costs, graph_model);
        req(graph.variables.size() == 2, "graph variable count");
        req(graph.rows.size() == 1, "graph row count");
        req(graph.edges.size() == 2, "active constraint creates both bipartite edges");
        req(graph.rows[0][1] == 1.0 && graph.rows[0][2] == 0.0 &&
                graph.rows[0][3] == 1.0,
            "row activity, dual placeholder, and density are represented");
    }

#ifdef MARKOV_CERO_ENABLE_ML
    // Collected feature values must be available to ml_gnn before this node's
    // strong-branch labels are computed. Both branches are feasible, so the
    // single fractional root candidate has a cold-start 0.5/0.5 pseudocost
    // ratio; its record must not contain same-node label information.
    {
        const fs::path log_path = fs::temp_directory_path() /
                                  "markov_cero_ml_feature_snapshot.sb.bin";
        std::error_code ec;
        fs::remove(log_path, ec);
        model::Model m;
        m.name = "ml_feature_snapshot";
        m.objective = {-1.0, -1.0};
        model::SparseMatrixBuilder builder(1, 2);
        builder.add(0, 0, 1.0);
        builder.add(0, 1, 1.0);
        m.matrix = builder.build();
        m.row_lower = {model::Bound::negative_infinity()};
        m.row_upper = {model::Bound::finite(1.5)};
        m.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
        m.variable_upper = {model::Bound::finite(3.0), model::Bound::finite(3.0)};
        m.variable_type = {model::VariableType::integer, model::VariableType::integer};
        m.row_name = {"capacity"};
        m.variable_name = {"x0", "x1"};
        m.validate();

        req(::setenv("MARKOV_CERO_SB_LOG", log_path.c_str(), 1) == 0,
            "set training log path");
        milp::Options options;
        options.branching_strategy = milp::BranchingStrategy::strong_branching;
        options.enable_cuts = false;
        options.enable_heuristics = false;
        options.max_nodes = 16;
        const auto solved = milp::solve(m, options);
        req(::unsetenv("MARKOV_CERO_SB_LOG") == 0, "clear training log path");
        if (solved.status != lp::reference::SolveStatus::optimal) {
            std::cerr << "feature snapshot fixture: " << lp::reference::to_string(solved.status)
                      << ": " << solved.message << '\n';
        }
        req(solved.status == lp::reference::SolveStatus::optimal,
            "feature snapshot fixture solves");

        std::ifstream log(log_path, std::ios::binary);
        char magic[8]{};
        std::uint32_t counts[3]{};
        log.read(magic, sizeof(magic));
        log.read(reinterpret_cast<char*>(counts), sizeof(counts));
        req(log.good() && std::string(magic, sizeof(magic)) == "MCONLOG3",
            "feature snapshot emits an MCONLOG3 record");
        req(counts[0] == 1, "fixture has one fractional candidate");
        double features[6]{};
        log.read(reinterpret_cast<char*>(features), sizeof(features));
        req(log.good(), "feature vector is complete");
        req(std::abs(features[2] - 0.5) < 1e-12 &&
                std::abs(features[3] - 0.5) < 1e-12,
            "training pseudocost features contain only pre-probe information");
        fs::remove(log_path, ec);
    }
#endif

    // The locked >200 fractional-candidate gate is exact: 200 remains
    // pseudo-cost; 201 invokes the scorer passed to this solve.
    {
        CountingScorer scorer;
        milp::MlBranchingTelemetry telemetry;
        const auto model_200 = make_milp(200, 2);
        std::vector<double> primal_200(200, 0.5);
        std::vector<milp::VariablePseudoCost> pc_200(200);
        (void)milp::select_branching_variable(primal_200, model_200.variable_type,
                                              pc_200, milp::BranchingStrategy::ml_gnn,
                                              1e-6, &model_200, &scorer, &telemetry);
        req(scorer.calls == 0, "200 candidates stay on pseudo-cost");

        const auto model_201 = make_milp(201, 2);
        std::vector<double> primal_201(201, 0.5);
        std::vector<milp::VariablePseudoCost> pc_201(201);
        (void)milp::select_branching_variable(primal_201, model_201.variable_type,
                                              pc_201, milp::BranchingStrategy::ml_gnn,
                                              1e-6, &model_201, &scorer, &telemetry);
        req(scorer.calls == 1, "201 candidates invoke ML scorer");
        req(telemetry.scored_nodes == 1 && telemetry.fallback_nodes == 1,
            "telemetry records scored and gate-fallback nodes");
        req(telemetry.candidates_scored == 201 &&
                telemetry.maximum_candidate_count == 201,
            "telemetry records genuine candidate scoring volume");
        req(telemetry.fallback_reason == "fractional_candidate_gate_not_met",
            "telemetry explains why 200 candidates did not activate ML");
    }

    // 2) Feature extraction produces finite, sensible features for candidates.
#ifndef MARKOV_CERO_ENABLE_ML
    std::cout << "ML runtime not compiled (MARKOV_CERO_ENABLE_ML=OFF); "
                 "fallback contract verified\n";
#endif



#ifdef MARKOV_CERO_ENABLE_ML
    if (const char* path = std::getenv("MARKOV_CERO_TEST_ONNX")) {
        milp::ml::OnnxBranchingScorer scorer(path);
        req(scorer.loaded(), "standard ONNX model loads");
        auto graph_model = make_milp(2, 0);
        graph_model.objective = {1.0, 2.0};
        graph_model.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
        graph_model.variable_upper = {model::Bound::finite(1.0), model::Bound::finite(1.0)};
        model::SparseMatrixBuilder builder(1, 2);
        builder.add(0, 0, 1.0);
        builder.add(0, 1, 1.0);
        graph_model.matrix = builder.build();
        graph_model.row_lower = {model::Bound::negative_infinity()};
        graph_model.row_upper = {model::Bound::finite(1.0)};
        std::vector<milp::VariablePseudoCost> pc(2);
        const auto graph = milp::extract_bipartite_features(
            {0.5, 0.5}, graph_model.variable_type, {0, 1}, pc, graph_model);
        const auto scores = scorer.score_graph(graph);
        req(scores.size() == 2, "GCN scores align with candidate nodes");
        const char* expected_env = std::getenv("MARKOV_CERO_TEST_ONNX_EXPECTED");
        req(expected_env != nullptr,
            "set MARKOV_CERO_TEST_ONNX_EXPECTED to ONNX Runtime scores for this model");
        std::istringstream expected_stream(expected_env);
        double expected0 = 0.0, expected1 = 0.0;
        char comma = '\0';
        req(static_cast<bool>(expected_stream >> expected0 >> comma >> expected1) && comma == ',',
            "ONNX Runtime reference scores must be formatted as score0,score1");
        req(std::isfinite(expected0) && std::isfinite(expected1),
            "ONNX Runtime reference scores are finite");
        const double parity_tolerance = 1e-4;
        if (std::abs(scores[0] - expected0) >= parity_tolerance ||
            std::abs(scores[1] - expected1) >= parity_tolerance) {
            std::cerr << "C++ GCN scores: " << scores[0] << ", " << scores[1]
                      << "; ONNX Runtime: " << expected0 << ", " << expected1 << '\n';
        }
        req(std::abs(scores[0] - expected0) < parity_tolerance &&
                std::abs(scores[1] - expected1) < parity_tolerance,
            "C++ GCN inference matches ONNX Runtime scores on the same graph");
    }
#endif

    std::cout << "ml branching tests passed\n";
    return 0;
}
