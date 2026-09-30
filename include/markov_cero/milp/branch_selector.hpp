#pragma once

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <array>
#include <string>
#include <vector>

namespace markov_cero::milp {

enum class BranchingStrategy { most_fractional, pseudo_cost, strong_branching, reliability, ml_gnn };

struct VariablePseudoCost {
    double down_sum{0.0};
    double up_sum{0.0};
    std::size_t down_count{0};
    std::size_t up_count{0};

    [[nodiscard]] double down_cost() const noexcept;
    [[nodiscard]] double up_cost() const noexcept;
    void record_down(double delta_obj, double fraction) noexcept;
    void record_up(double delta_obj, double fraction) noexcept;
};

// W2 / D-04: feature vector for one fractional variable at one B&B node.
// All features are raw doubles; normalization happens in the scorer.
struct NodeFeatureVector {
    std::size_t variable = 0;      // column index
    double fractionality = 0.0;    // min(x - floor, ceil - x)
    double objective_coefficient = 0.0;
    double pseudocost_down_ratio = 0.0;  // down_cost / (down_cost + up_cost)
    double pseudocost_up_ratio = 0.0;    // up_cost / (down_cost + up_cost)
    double bound_width = 0.0;      // ub - lb (may be large/infinite -> capped)
    double column_density = 0.0;   // nnz in column / rows
};

// Bipartite graph inputs for the W2 graph scorer. Edges are stored in
// candidate-variable / row coordinates and carry the normalized A_ij value.
// Row features are [normalized side, normalized primal activity, normalized
// dual multiplier, row density]. Only active rows are included in the graph.
struct BipartiteEdge {
    std::size_t variable_node{0};
    std::size_t row_node{0};
    double coefficient{0.0};
};

struct BipartiteGraphFeatures {
    std::vector<NodeFeatureVector> variables;
    std::vector<std::array<double, 4>> rows;
    std::vector<BipartiteEdge> edges;
};

// W2 / D-04 + D-18: pluggable branching scorer. The default is pseudo-cost;
// the ONNX GCN scorer (compiled only under MARKOV_CERO_ENABLE_ML) implements
// this interface and returns a score per candidate; the highest score wins.
class IBranchingScorer {
  public:
    virtual ~IBranchingScorer() = default;

    // Extract per-candidate features for logging/inference.
    [[nodiscard]] virtual std::vector<NodeFeatureVector>
    extract_features(const std::vector<double>& primal,
                     const std::vector<model::VariableType>& types,
                     const std::vector<std::size_t>& candidates,
                     const std::vector<VariablePseudoCost>& pseudo_costs,
                     const model::Model& model) const;

    // Score per candidate (parallel to candidates). Higher = branch here.
    [[nodiscard]] virtual std::vector<double>
    score_candidates(const std::vector<NodeFeatureVector>& features) const = 0;

    // Graph-aware path used by the ML selector. The default preserves
    // compatibility for simple scorers while graph scorers override it.
    [[nodiscard]] virtual std::vector<double>
    score_graph(const BipartiteGraphFeatures& graph) const {
        return score_candidates(graph.variables);
    }
};

struct MlBranchingTelemetry {
    std::size_t eligible_nodes{0};
    std::size_t scored_nodes{0};
    std::size_t fallback_nodes{0};
    std::size_t candidates_scored{0};
    std::size_t maximum_candidate_count{0};
    std::string fallback_reason;
};

// Strong-branching score logger for offline training (D-05): accumulates
// (features, strong-branching scores) pairs at every node where strong
// branching was evaluated. Written to disk by the training-data collector.
class TrainingLogger {
  public:
    struct Record {
        std::vector<NodeFeatureVector> features;
        std::vector<std::array<double, 4>> row_features;
        std::vector<BipartiteEdge> edges;
        std::vector<double> sb_scores;  // Achterberg product scores from SB
    };

    void add(Record record) { records_.push_back(std::move(record)); }
    [[nodiscard]] const std::vector<Record>& records() const noexcept { return records_; }
    [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }
    void clear() { records_.clear(); }

  private:
    std::vector<Record> records_;
};

[[nodiscard]] std::vector<std::size_t>
find_fractional_variables(const std::vector<double>& primal,
                          const std::vector<model::VariableType>& types,
                          double integrality_tol = 1e-6);

// MIP-01 contract §4: the branch-partition certificate for one split value.
// floor/ceil bound the two children; a child is valid only when its tightened
// bound still overlaps the parent domain (1e-9 production slack, matching the
// serial gates). floor >= ceil is a degenerate split and must be rejected
// rather than pushed; both gates invalid means the parent holds no integer
// point in this variable (conclusive emptiness, recorded never pruned).
struct SplitPartition {
    double floor_value{0.0};
    double ceil_value{0.0};
    bool down_valid{false};
    bool up_valid{false};
};

[[nodiscard]] SplitPartition evaluate_split(double branch_val,
                                            const model::Bound& parent_lower,
                                            const model::Bound& parent_upper);

[[nodiscard]] BipartiteGraphFeatures extract_bipartite_features(
    const std::vector<double>& primal,
    const std::vector<model::VariableType>& types,
    const std::vector<std::size_t>& candidates,
    const std::vector<VariablePseudoCost>& pseudo_costs,
    const model::Model& model,
    const std::vector<double>& row_duals = {});

[[nodiscard]] std::size_t select_most_fractional(const std::vector<double>& primal,
                                                 const std::vector<std::size_t>& candidates);

[[nodiscard]] std::size_t select_pseudo_cost(const std::vector<double>& primal,
                                             const std::vector<std::size_t>& candidates,
                                             const std::vector<VariablePseudoCost>& pseudo_costs);

[[nodiscard]] std::size_t
select_branching_variable(const std::vector<double>& primal,
                          const std::vector<model::VariableType>& types,
                          const std::vector<VariablePseudoCost>& pseudo_costs,
                          BranchingStrategy strategy, double integrality_tol = 1e-6,
                          const model::Model* feature_model = nullptr,
                          const IBranchingScorer* scorer = nullptr,
                          MlBranchingTelemetry* telemetry = nullptr,
                          const std::vector<double>* row_duals = nullptr);

} // namespace markov_cero::milp
