#pragma once
#include <memory>

#include "markov_cero/milp/branch_selector.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace markov_cero::milp::ml {

// W2 / D-04 / D-18: branching scorer backed by a standard ONNX model.
// Compiled ONLY when MARKOV_CERO_ENABLE_ML=ON. The built-in inference path
// reads float initializers from the exported GCN graph and evaluates its
// fixed two-layer bipartite message-passing architecture without linking a
// solver or ML runtime library.
class OnnxBranchingScorer final : public IBranchingScorer {
  public:
    // Feature-only construction: extraction works without a model; only
    // score_candidates requires a loaded model (loaded() == false).
    OnnxBranchingScorer();

    // Load the model file. Throws std::runtime_error when the file is
    // missing or malformed; the caller (solver wiring) treats any throw as
    // "fall back to pseudo_cost silently" per the LOCKED activation contract.
    explicit OnnxBranchingScorer(const std::string& model_path);

    ~OnnxBranchingScorer() override;

    // Per-candidate compatibility entry point; graph-aware dispatch uses
    // score_graph() below. Scores are raw ranking scores.
    [[nodiscard]] std::vector<double>
    score_candidates(const std::vector<NodeFeatureVector>& features) const override;

    [[nodiscard]] std::vector<double>
    score_graph(const BipartiteGraphFeatures& graph) const override;

    [[nodiscard]] bool loaded() const noexcept { return loaded_; }

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool loaded_{false};
};

// Aggregates per-instance feature/score dumps for the training-data
// collector (D-05). Compiled under the ML flag alongside the scorer.
void log_sb_record(TrainingLogger& logger,
                   const std::vector<NodeFeatureVector>& features,
                   const std::vector<double>& sb_scores);

// W2/D-05: static (context-free) feature extraction used by the MILP solver's
// training logger — identical logic to the IBranchingScorer default, exposed
// statically so the solver does not need a scorer instance to log.
[[nodiscard]] std::vector<NodeFeatureVector> extract_features_static(
    const std::vector<double>& primal,
    const std::vector<model::VariableType>& types,
    const model::Model& model,
    const std::vector<VariablePseudoCost>& pseudo_costs);

// Append one graph training record (MCONLOG3): counts, variable nodes,
// row nodes, variable-row edges, and aligned strong-branching scores.
void append_sb_record(std::ostream& out,
                      const BipartiteGraphFeatures& graph,
                      const std::vector<double>& sb_scores);

} // namespace markov_cero::milp::ml
