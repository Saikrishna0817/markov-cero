#pragma once
#include "markov_cero/milp/gap.hpp"
#include "markov_cero/milp/milp_solver.hpp"

#include "markov_cero/core/solve_context.hpp"
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

namespace markov_cero::milp::detail {
struct ScopedSearchTimer {
    double& total;
    std::chrono::steady_clock::time_point started{std::chrono::steady_clock::now()};
    explicit ScopedSearchTimer(double& destination) : total(destination) {}
    ~ScopedSearchTimer() {
        total += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    }
};
inline void record_optimizer_cut_notes(Result& result, std::size_t node,
    const std::vector<Cut>& cuts, const std::vector<double>& primal) {
    for (const auto& cut : cuts) {
        if (cut.coefficients.size() != primal.size()) continue;
        double lhs = 0.0;
        for (std::size_t j = 0; j < primal.size(); ++j)
            lhs += cut.coefficients[j] * primal[j];
        verify::MipObligation note;
        note.kind = verify::MipObligationKind::cut;
        note.node = node;
        note.coefficients = cut.coefficients;
        note.rhs = cut.rhs;
        note.observed_lhs = lhs;
        result.obligations.push_back(std::move(note));
    }
}
class NodeFrontier {
  public:
    explicit NodeFrontier(NodeSelection policy) : comparator_{policy} {}

    void push(std::shared_ptr<BranchNode> node) {
        if (!node) {
            return;
        }
        if (heap_.size() >= maximum_size_) {
            capacity_exhausted_ = true;
            minimum_dropped_bound_ = std::min(minimum_dropped_bound_, node->lower_bound);
            return;
        }
        heap_.push_back(std::move(node));
        std::push_heap(heap_.begin(), heap_.end(), comparator_);
        peak_size_ = std::max(peak_size_, heap_.size());
    }

    void set_maximum_size(std::size_t maximum_size) { maximum_size_ = maximum_size; }
    [[nodiscard]] bool capacity_exhausted() const { return capacity_exhausted_; }
    [[nodiscard]] double minimum_dropped_bound() const { return minimum_dropped_bound_; }
    /// Peak live frontier size since construction (W01/IR-19 evidence).
    [[nodiscard]] std::size_t peak_size() const { return peak_size_; }

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
        double bound = minimum_dropped_bound_;
        if (heap_.empty()) {
            return bound;
        }
        if (comparator_.policy == NodeSelection::best_bound) {
            return std::min(bound, heap_.front()->lower_bound);
        }
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
    std::size_t maximum_size_{std::numeric_limits<std::size_t>::max()};
    double minimum_dropped_bound_{std::numeric_limits<double>::infinity()};
    bool capacity_exhausted_{false};
    std::size_t peak_size_{0};
};


struct Search {
const model::Model& model;
const Options& input_options;
Search(const model::Model& m, const Options& o): model(m), input_options(o) {}
std::chrono::steady_clock::time_point start_time{};
markov_cero::milp::Options options{};
markov_cero::milp::Result result{};
const markov_cero::milp::IBranchingScorer * branching_scorer{};
std::unique_ptr<markov_cero::milp::IBranchingScorer> owned_scorer{};
std::basic_ofstream<char> sb_log_stream{};
std::basic_ofstream<char> * sb_log_file{};
std::size_t next_node_id{};
double best_upper_bound{};
double best_lower_bound{};
std::vector<double> best_primal{};
std::vector<markov_cero::milp::VariablePseudoCost> pseudo_costs{};
markov_cero::model::Model root_model{};
markov_cero::milp::NodeLpResult root_lp{};
std::optional<markov_cero::lp::dual::BasisState> current_basis{};
std::vector<double> current_primal{};
std::vector<double> current_row_dual{};
double current_obj{};
std::vector<markov_cero::milp::Cut> root_cut_list{};
NodeFrontier queue {NodeSelection::best_bound};
std::size_t unsolved_node_lps{};
double min_unsolved_bound{};
std::basic_string<char> stop_reason{};
std::shared_ptr<BranchNode> node;
NodeLpResult node_lp_res;
model::Model node_model;
NodeBounds::MaterializationScratch node_bounds_scratch;
Result run();
Result finish();
bool initialize();
bool root_relaxation();
bool root_branching();
bool node_relaxation();
bool separate_cuts();
bool branch();
};
}
