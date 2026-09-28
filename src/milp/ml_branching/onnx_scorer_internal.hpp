#pragma once
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
namespace detail_onnx_scorer {}
namespace detail_onnx_scorer {
constexpr std::size_t kVarFeatures = 6;
}
namespace detail_onnx_scorer {
constexpr std::size_t kRowFeatures = 4;
}
namespace detail_onnx_scorer {
constexpr std::size_t kHidden = 32;
}
namespace detail_onnx_scorer {
struct ProtoField {
    std::uint32_t number{};
    std::uint8_t wire{};
    std::uint64_t integer{};
    const char* bytes{};
    std::size_t size{};
};
}
namespace detail_onnx_scorer {
struct TensorInit {
    std::string name;
    std::vector<std::uint64_t> dims;
    std::uint64_t data_type{};
    const char* raw{};
    std::size_t raw_size{};
    std::vector<float> packed_float;
};
}
namespace detail_onnx_scorer {
using TensorMap = std::unordered_map<std::string, TensorInit>;
}
struct OnnxBranchingScorer::Impl {
    static constexpr auto kHidden = detail_onnx_scorer::kHidden;
    static constexpr auto kVarFeatures = detail_onnx_scorer::kVarFeatures;
    static constexpr auto kRowFeatures = detail_onnx_scorer::kRowFeatures;
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
namespace detail_onnx_scorer { std::uint64_t read_varint(const char* data, std::size_t size, std::size_t& off); }
namespace detail_onnx_scorer { bool next_field(const char* data, std::size_t size, std::size_t& off,
                ProtoField& field); }
namespace detail_onnx_scorer { TensorInit parse_tensor(const char* data, std::size_t size); }
namespace detail_onnx_scorer { void parse_graph(const char* data, std::size_t size, TensorMap& tensors); }
namespace detail_onnx_scorer { TensorMap parse_onnx(const std::vector<char>& blob); }
namespace detail_onnx_scorer { std::size_t tensor_count(const TensorInit& t); }
namespace detail_onnx_scorer { std::vector<float> tensor_values(const TensorInit& t); }
namespace detail_onnx_scorer { void load_tensor(const TensorMap& tensors, const std::string& name,
                 const std::vector<std::uint64_t>& shape, float* dst, std::size_t count); }
namespace detail_onnx_scorer { void load_unnamed_matrix(const TensorMap& tensors,
                         const std::vector<std::uint64_t>& shape, float* dst,
                         const std::string& description); }
namespace detail_onnx_scorer { float relu(float x) noexcept; }
void log_sb_record(TrainingLogger& logger,
                   const std::vector<NodeFeatureVector>& features,
                   const std::vector<double>& sb_scores);
std::vector<NodeFeatureVector> extract_features_static(
    const std::vector<double>& primal,
    const std::vector<model::VariableType>& types,
    const model::Model& model,
    const std::vector<VariablePseudoCost>& pseudo_costs);
void append_sb_record(std::ostream& out,
                      const BipartiteGraphFeatures& graph,
                      const std::vector<double>& sb_scores);
}
