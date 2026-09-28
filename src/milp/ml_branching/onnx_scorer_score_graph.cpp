#include "onnx_scorer_internal.hpp"
namespace markov_cero::milp::ml {
using namespace detail_onnx_scorer;
std::vector<double> OnnxBranchingScorer::score_graph(
    const BipartiteGraphFeatures& graph) const {
    if (!loaded_) throw std::runtime_error("ml branching: ONNX scorer not loaded");
    const std::size_t nv = graph.variables.size(), nr = graph.rows.size();
    if (nv == 0) return {};
    std::vector<std::array<float, kVarFeatures>> x(nv);
    std::vector<std::array<float, kRowFeatures>> r(nr);
    for (std::size_t i = 0; i < nv; ++i) {
        const auto& f = graph.variables[i];
        const double raw[kVarFeatures] = {f.fractionality, f.objective_coefficient,
            f.pseudocost_down_ratio, f.pseudocost_up_ratio, f.bound_width, f.column_density};
        for (std::size_t k = 0; k < kVarFeatures; ++k) {
            if (!std::isfinite(raw[k])) throw std::runtime_error("ml branching: non-finite variable feature");
            x[i][k] = (static_cast<float>(raw[k]) - impl_->var_center[k]) / impl_->var_scale[k];
        }
    }
    for (std::size_t i = 0; i < nr; ++i) {
        for (std::size_t k = 0; k < kRowFeatures; ++k) {
            const double raw = graph.rows[i][k];
            if (!std::isfinite(raw)) throw std::runtime_error("ml branching: non-finite row feature");
            r[i][k] = (static_cast<float>(raw) - impl_->row_center[k]) / impl_->row_scale[k];
        }
    }
    std::vector<float> dv(nv, 0.0f), dr(nr, 0.0f);
    for (const auto& e : graph.edges) {
        if (e.variable_node >= nv || e.row_node >= nr || !std::isfinite(e.coefficient))
            throw std::runtime_error("ml branching: invalid bipartite edge");
        const float a = std::abs(static_cast<float>(e.coefficient));
        dv[e.variable_node] += a;
        dr[e.row_node] += a;
    }
    std::vector<std::array<float, kHidden>> row_agg(nr), row_h(nr), var_agg(nv), var_h(nv);
    for (const auto& e : graph.edges) {
        const float norm = static_cast<float>(e.coefficient) /
            std::sqrt(std::max(dv[e.variable_node], 1e-12f) * std::max(dr[e.row_node], 1e-12f));
        for (std::size_t h = 0; h < kHidden; ++h) {
            float msg = 0.0f;
            for (std::size_t k = 0; k < kVarFeatures; ++k)
                msg += x[e.variable_node][k] * impl_->var_to_row_w[k * kHidden + h];
            row_agg[e.row_node][h] += norm * msg;
        }
    }
    for (std::size_t i = 0; i < nr; ++i) for (std::size_t h = 0; h < kHidden; ++h) {
        float sum = impl_->row_self_b[h] + row_agg[i][h];
        for (std::size_t k = 0; k < kRowFeatures; ++k)
            sum += impl_->row_self_w[h * kRowFeatures + k] * r[i][k];
        row_h[i][h] = relu(sum);
    }
    for (const auto& e : graph.edges) {
        const float norm = static_cast<float>(e.coefficient) /
            std::sqrt(std::max(dv[e.variable_node], 1e-12f) * std::max(dr[e.row_node], 1e-12f));
        for (std::size_t h = 0; h < kHidden; ++h) {
            float msg = 0.0f;
            for (std::size_t k = 0; k < kHidden; ++k)
                msg += row_h[e.row_node][k] * impl_->row_to_var_w[k * kHidden + h];
            var_agg[e.variable_node][h] += norm * msg;
        }
    }
    std::vector<double> scores(nv);
    for (std::size_t i = 0; i < nv; ++i) {
        for (std::size_t h = 0; h < kHidden; ++h) {
            float sum = impl_->var_self_b[h] + var_agg[i][h];
            for (std::size_t k = 0; k < kVarFeatures; ++k)
                sum += impl_->var_self_w[h * kVarFeatures + k] * x[i][k];
            var_h[i][h] = relu(sum);
        }
        float output = impl_->out2_b;
        for (std::size_t j = 0; j < kHidden; ++j) {
            float hidden = impl_->out1_b[j];
            for (std::size_t k = 0; k < kHidden; ++k)
                hidden += impl_->out1_w[j * kHidden + k] * var_h[i][k];
            output += impl_->out2_w[j] * relu(hidden);
        }
        if (!std::isfinite(output)) throw std::runtime_error("ml branching: non-finite GCN score");
        scores[i] = static_cast<double>(output);
    }
    return scores;
}
void log_sb_record(TrainingLogger& logger,
                   const std::vector<NodeFeatureVector>& features,
                   const std::vector<double>& sb_scores) {
    TrainingLogger::Record record;
    record.features = features;
    record.sb_scores = sb_scores;
    logger.add(std::move(record));
}
std::vector<NodeFeatureVector> extract_features_static(
    const std::vector<double>& primal,
    const std::vector<model::VariableType>& types,
    const model::Model& model,
    const std::vector<VariablePseudoCost>& pseudo_costs) {
    const auto candidates = find_fractional_variables(primal, types, 1e-6);
    class FeatureOnlyScorer final : public IBranchingScorer {
      public:
        std::vector<double> score_candidates(const std::vector<NodeFeatureVector>&) const override { return {}; }
    } scorer;
    return scorer.extract_features(primal, types, candidates, pseudo_costs, model);
}
void append_sb_record(std::ostream& out,
                      const BipartiteGraphFeatures& graph,
                      const std::vector<double>& sb_scores) {
    const std::size_t n = graph.variables.size();
    if (n == 0 || sb_scores.size() != n ||
        n > std::numeric_limits<std::uint32_t>::max() ||
        graph.rows.size() > std::numeric_limits<std::uint32_t>::max() ||
        graph.edges.size() > std::numeric_limits<std::uint32_t>::max()) return;
    for (const auto& edge : graph.edges)
        if (edge.variable_node >= n || edge.row_node >= graph.rows.size() ||
            !std::isfinite(edge.coefficient)) return;
    for (const auto& f : graph.variables) {
        const double vals[kVarFeatures] = {f.fractionality, f.objective_coefficient,
            f.pseudocost_down_ratio, f.pseudocost_up_ratio, f.bound_width, f.column_density};
        if (!std::all_of(std::begin(vals), std::end(vals),
                         [](double x) { return std::isfinite(x); })) return;
    }
    for (const auto& row : graph.rows)
        if (!std::all_of(row.begin(), row.end(),
                         [](double x) { return std::isfinite(x); })) return;
    if (!std::all_of(sb_scores.begin(), sb_scores.end(),
                     [](double x) { return std::isfinite(x); })) return;
    out.write("MCONLOG3", 8);
    const std::uint32_t counts[3] = {static_cast<std::uint32_t>(n),
        static_cast<std::uint32_t>(graph.rows.size()),
        static_cast<std::uint32_t>(graph.edges.size())};
    out.write(reinterpret_cast<const char*>(counts), sizeof(counts));
    for (const auto& f : graph.variables) {
        const double values[kVarFeatures] = {f.fractionality, f.objective_coefficient,
            f.pseudocost_down_ratio, f.pseudocost_up_ratio, f.bound_width, f.column_density};
        out.write(reinterpret_cast<const char*>(values), sizeof(values));
    }
    for (const auto& row : graph.rows)
        out.write(reinterpret_cast<const char*>(row.data()), kRowFeatures * sizeof(double));
    for (const auto& edge : graph.edges) {
        const std::uint32_t ids[2] = {static_cast<std::uint32_t>(edge.variable_node),
                                      static_cast<std::uint32_t>(edge.row_node)};
        out.write(reinterpret_cast<const char*>(ids), sizeof(ids));
        out.write(reinterpret_cast<const char*>(&edge.coefficient), sizeof(double));
    }
    out.write(reinterpret_cast<const char*>(sb_scores.data()),
              static_cast<std::streamsize>(n * sizeof(double)));
    out.flush();
}
}
