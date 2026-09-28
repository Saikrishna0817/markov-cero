#include "onnx_scorer_internal.hpp"
namespace markov_cero::milp::ml {
using namespace detail_onnx_scorer;
namespace detail_onnx_scorer {
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
}

namespace detail_onnx_scorer {
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
}

namespace detail_onnx_scorer {
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
}

namespace detail_onnx_scorer {
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
}

namespace detail_onnx_scorer {
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
}

namespace detail_onnx_scorer {
std::size_t tensor_count(const TensorInit& t) {
    std::size_t n = 1;
    for (auto dim : t.dims) {
        if (dim == 0) return 0;
        if (dim > std::numeric_limits<std::size_t>::max() / n)
            throw std::runtime_error("ml branching: ONNX initializer dimension overflow");
        n *= static_cast<std::size_t>(dim);
    }
    return n;
}
}

namespace detail_onnx_scorer {
std::vector<float> tensor_values(const TensorInit& t) {
    const std::size_t count = tensor_count(t);
    if (count > 16U * 1024U * 1024U / sizeof(float)) throw std::runtime_error("ONNX tensor budget");
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
}

namespace detail_onnx_scorer {
void load_tensor(const TensorMap& tensors, const std::string& name,
                 const std::vector<std::uint64_t>& shape, float* dst, std::size_t count) {
    const auto it = tensors.find(name);
    if (it == tensors.end() || it->second.dims != shape || tensor_count(it->second) != count)
        throw std::runtime_error("ml branching: missing or malformed ONNX weight " + name);
    const auto values = tensor_values(it->second);
    if (!std::all_of(values.begin(), values.end(), [](float v) { return std::isfinite(v); }))
        throw std::runtime_error("ml branching: non-finite ONNX weights");
    std::copy(values.begin(), values.end(), dst);
}
}

namespace detail_onnx_scorer {
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
    if (!std::all_of(values.begin(), values.end(), [](float v) { return std::isfinite(v); }))
        throw std::runtime_error("ml branching: non-finite ONNX weights");
    std::copy(values.begin(), values.end(), dst);
}
}

namespace detail_onnx_scorer {
float relu(float x) noexcept { return x > 0.0f ? x : 0.0f; }
}

OnnxBranchingScorer::OnnxBranchingScorer() : impl_(new Impl()) {}
OnnxBranchingScorer::OnnxBranchingScorer(const std::string& model_path) : impl_(new Impl()) {
    std::ifstream in(model_path, std::ios::binary);
    if (!in) throw std::runtime_error("ml branching: cannot open ONNX model: " + model_path);
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size < 0 || size > 16 * 1024 * 1024) throw std::runtime_error("ONNX input exceeds 16 MiB");
    in.seekg(0);
    std::vector<char> blob(static_cast<std::size_t>(size));
    if (!in.read(blob.data(), static_cast<std::streamsize>(blob.size())))
        throw std::runtime_error("cannot read ONNX input");
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
OnnxBranchingScorer::~OnnxBranchingScorer() = default;
std::vector<double> OnnxBranchingScorer::score_candidates(
    const std::vector<NodeFeatureVector>& features) const {
    BipartiteGraphFeatures graph;
    graph.variables = features;
    return score_graph(graph);
}
}
