// W2 / D-04 / D-18: small ONNX protobuf reader and fixed-architecture GCN
// inference implementation. The file is a standard ONNX graph; this
// interpreter consumes its learned TensorProto initializers and evaluates
// the corresponding two-layer bipartite GCN without external ML libraries.
#include "markov_cero/milp/ml_branching/onnx_scorer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ostream>

namespace markov_cero::milp::ml {
namespace {

constexpr std::size_t kVarFeatures = 6;
constexpr std::size_t kRowFeatures = 4;
constexpr std::size_t kHidden = 32;

struct ProtoField {
    std::uint32_t number{};
    std::uint8_t wire{};
    std::uint64_t integer{};
    const char* bytes{};
    std::size_t size{};
};

std::uint64_t read_varint(const char* data, std::size_t size, std::size_t& off) {
    std::uint64_t result = 0;
    unsigned shift = 0;
    while (off < size && shift < 64) {
        const auto b = static_cast<std::uint8_t>(data[off++]);
        result |= static_cast<std::uint64_t>(b & 0x7fU) << shift;
        if ((b & 0x80U) == 0) return result;
        shift += 7;
    }
    throw std::runtime_error("ml branching: malformed ONNX protobuf varint");
}

bool next_field(const char* data, std::size_t size, std::size_t& off,
                ProtoField& field) {
    if (off == size) return false;
    const std::uint64_t key = read_varint(data, size, off);
    field.number = static_cast<std::uint32_t>(key >> 3U);
    field.wire = static_cast<std::uint8_t>(key & 7U);
    if (field.number == 0) throw std::runtime_error("ml branching: invalid ONNX field 0");
    switch (field.wire) {
        case 0: field.integer = read_varint(data, size, off); break;
        case 1:
            if (size - off < 8) throw std::runtime_error("ml branching: truncated ONNX field");
            field.bytes = data + off; field.size = 8; off += 8; break;
        case 2: {
            const auto len = read_varint(data, size, off);
            if (len > size - off) throw std::runtime_error("ml branching: truncated ONNX blob");
            field.bytes = data + off; field.size = static_cast<std::size_t>(len);
            off += field.size;
            break;
        }
        case 5:
            if (size - off < 4) throw std::runtime_error("ml branching: truncated ONNX field");
            field.bytes = data + off; field.size = 4; off += 4; break;
        default: throw std::runtime_error("ml branching: unsupported ONNX wire type");
    }
    return true;
}

struct TensorInit {
    std::string name;
    std::vector<std::uint64_t> dims;
    std::uint64_t data_type{};
    const char* raw{};
    std::size_t raw_size{};
    std::vector<float> packed_float;
};

TensorInit parse_tensor(const char* data, std::size_t size) {
    TensorInit t;
    std::size_t off = 0;
    ProtoField f;
    while (next_field(data, size, off, f)) {
        if (f.number == 1) {
            if (f.wire == 0) t.dims.push_back(f.integer);
            else if (f.wire == 2) {
                std::size_t p = 0;
                while (p < f.size) t.dims.push_back(read_varint(f.bytes, f.size, p));
            }
        } else if (f.number == 2 && f.wire == 0) {
            t.data_type = f.integer;
        } else if (f.number == 4) {
            if (f.wire == 5) {
                float value{}; std::memcpy(&value, f.bytes, sizeof(value));
                t.packed_float.push_back(value);
            } else if (f.wire == 2) {
                if (f.size % sizeof(float) != 0) throw std::runtime_error("ml branching: bad float tensor");
                for (std::size_t p = 0; p < f.size; p += sizeof(float)) {
                    float value{}; std::memcpy(&value, f.bytes + p, sizeof(value));
                    t.packed_float.push_back(value);
                }
            }
        } else if (f.number == 8 && f.wire == 2) {
            t.name.assign(f.bytes, f.size);
        } else if (f.number == 9 && f.wire == 2) {
            t.raw = f.bytes; t.raw_size = f.size;
        }
    }
    if (t.data_type != 1) throw std::runtime_error("ml branching: expected float ONNX initializers");
    return t;
}

using TensorMap = std::unordered_map<std::string, TensorInit>;

void parse_graph(const char* data, std::size_t size, TensorMap& tensors) {
    std::size_t off = 0;
    ProtoField f;
    while (next_field(data, size, off, f)) {
        // GraphProto.initializer is field 5.
        if (f.number == 5 && f.wire == 2) {
            TensorInit tensor = parse_tensor(f.bytes, f.size);
            if (tensor.name.empty() || !tensors.emplace(tensor.name, std::move(tensor)).second)
                throw std::runtime_error("ml branching: duplicate/unnamed ONNX initializer");
        }
    }
}

TensorMap parse_onnx(const std::vector<char>& blob) {
    TensorMap tensors;
    std::size_t off = 0;
    ProtoField f;
    bool found_graph = false;
    while (next_field(blob.data(), blob.size(), off, f)) {
        // ModelProto.graph is field 7.
        if (f.number == 7 && f.wire == 2) {
            parse_graph(f.bytes, f.size, tensors);
            found_graph = true;
            break;
        }
    }
    if (!found_graph || tensors.empty()) throw std::runtime_error("ml branching: ONNX graph has no weights");
    return tensors;
}

std::size_t tensor_count(const TensorInit& t) {
    std::size_t n = 1;
    for (auto dim : t.dims) {
        if (dim > std::numeric_limits<std::size_t>::max() / n)
            throw std::runtime_error("ml branching: ONNX initializer dimension overflow");
        n *= static_cast<std::size_t>(dim);
    }
    return n;
}

std::vector<float> tensor_values(const TensorInit& t) {
    const std::size_t count = tensor_count(t);
    if (t.raw_size != 0) {
        if (t.raw_size != count * sizeof(float))
            throw std::runtime_error("ml branching: ONNX initializer byte count mismatch: " + t.name);
        std::vector<float> out(count);
        std::memcpy(out.data(), t.raw, t.raw_size);
        return out;
    }
    if (t.packed_float.size() != count)
        throw std::runtime_error("ml branching: ONNX initializer data missing: " + t.name);
    return t.packed_float;
}

void load_tensor(const TensorMap& tensors, const std::string& name,
                 const std::vector<std::uint64_t>& shape, float* dst, std::size_t count) {
    const auto it = tensors.find(name);
    if (it == tensors.end() || it->second.dims != shape || tensor_count(it->second) != count)
        throw std::runtime_error("ml branching: missing or malformed ONNX weight " + name);
    const auto values = tensor_values(it->second);
    std::copy(values.begin(), values.end(), dst);
}

void load_unnamed_matrix(const TensorMap& tensors,
                         const std::vector<std::uint64_t>& shape, float* dst,
                         const std::string& description) {
    const TensorInit* found = nullptr;
    for (const auto& item : tensors) {
        const auto& t = item.second;
        if (t.dims == shape && item.first.rfind("onnx::", 0) == 0) {
            if (found != nullptr) throw std::runtime_error("ml branching: ambiguous ONNX " + description);
            found = &t;
        }
    }
    if (found == nullptr) throw std::runtime_error("ml branching: missing ONNX " + description);
    const auto values = tensor_values(*found);
    std::copy(values.begin(), values.end(), dst);
}

float relu(float x) noexcept { return x > 0.0f ? x : 0.0f; }

} // namespace

struct OnnxBranchingScorer::Impl {
    std::array<float, kHidden * kVarFeatures> var_self_w{};
    std::array<float, kHidden> var_self_b{};
    std::array<float, kHidden * kRowFeatures> row_self_w{};
    std::array<float, kHidden> row_self_b{};
    std::array<float, kVarFeatures * kHidden> var_to_row_w{};
    std::array<float, kHidden * kHidden> row_to_var_w{};
    std::array<float, kHidden * kHidden> out1_w{};
    std::array<float, kHidden> out1_b{};
    std::array<float, kHidden> out2_w{};
    float out2_b{};
    std::array<float, kVarFeatures> var_center{};
    std::array<float, kVarFeatures> var_scale{};
    std::array<float, kRowFeatures> row_center{};
    std::array<float, kRowFeatures> row_scale{};
};

OnnxBranchingScorer::OnnxBranchingScorer() : impl_(new Impl()) {}

OnnxBranchingScorer::OnnxBranchingScorer(const std::string& model_path) : impl_(new Impl()) {
    std::ifstream in(model_path, std::ios::binary);
    if (!in) throw std::runtime_error("ml branching: cannot open ONNX model: " + model_path);
    std::vector<char> blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto tensors = parse_onnx(blob);
    load_tensor(tensors, "model.var_self.weight", {kHidden, kVarFeatures}, impl_->var_self_w.data(), impl_->var_self_w.size());
    load_tensor(tensors, "model.var_self.bias", {kHidden}, impl_->var_self_b.data(), impl_->var_self_b.size());
    load_tensor(tensors, "model.row_self.weight", {kHidden, kRowFeatures}, impl_->row_self_w.data(), impl_->row_self_w.size());
    load_tensor(tensors, "model.row_self.bias", {kHidden}, impl_->row_self_b.data(), impl_->row_self_b.size());
    load_unnamed_matrix(tensors, {kVarFeatures, kHidden}, impl_->var_to_row_w.data(), "variable-to-row matrix");
    load_unnamed_matrix(tensors, {kHidden, kHidden}, impl_->row_to_var_w.data(), "row-to-variable matrix");
    load_tensor(tensors, "model.var_out.0.weight", {kHidden, kHidden}, impl_->out1_w.data(), impl_->out1_w.size());
    load_tensor(tensors, "model.var_out.0.bias", {kHidden}, impl_->out1_b.data(), impl_->out1_b.size());
    load_tensor(tensors, "model.var_out.2.weight", {1, kHidden}, impl_->out2_w.data(), impl_->out2_w.size());
    load_tensor(tensors, "model.var_out.2.bias", {1}, &impl_->out2_b, 1);
    load_tensor(tensors, "variable_center", {kVarFeatures}, impl_->var_center.data(), impl_->var_center.size());
    load_tensor(tensors, "variable_scale", {kVarFeatures}, impl_->var_scale.data(), impl_->var_scale.size());
    load_tensor(tensors, "row_center", {kRowFeatures}, impl_->row_center.data(), impl_->row_center.size());
    load_tensor(tensors, "row_scale", {kRowFeatures}, impl_->row_scale.data(), impl_->row_scale.size());
    for (float s : impl_->var_scale) if (!std::isfinite(s) || s == 0.0f) throw std::runtime_error("ml branching: invalid variable scaler");
    for (float s : impl_->row_scale) if (!std::isfinite(s) || s == 0.0f) throw std::runtime_error("ml branching: invalid row scaler");
    loaded_ = true;
}

OnnxBranchingScorer::~OnnxBranchingScorer() { delete impl_; }

std::vector<double> OnnxBranchingScorer::score_candidates(
    const std::vector<NodeFeatureVector>& features) const {
    BipartiteGraphFeatures graph;
    graph.variables = features;
    return score_graph(graph);
}

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

} // namespace markov_cero::milp::ml
