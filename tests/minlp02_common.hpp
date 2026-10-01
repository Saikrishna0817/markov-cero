#pragma once

// MINLP-02 shared fixtures (docs/contracts/minlp-proof-replay.md section 7):
// source builders and helpers shared by the proof, attack and enumeration
// suites. Each suite keeps its own main() under the 300-line source cap.

#include "markov_cero/api/solve.hpp"
#include "markov_cero/model/model_snapshot.hpp"
#include "markov_cero/verify/oa_proof.hpp"

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>

namespace minlp02 {

using markov_cero::model::Bound;
using markov_cero::model::ObjectiveSense;
using markov_cero::model::VariableType;

inline void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

inline bool contains(const std::string& text, const std::string& fragment) {
    return text.find(fragment) != std::string::npos;
}

inline std::string fingerprint_of(const markov_cero::model::Model& model) {
    return std::to_string(markov_cero::model::hash_model(model).fingerprint());
}

// Healthy case A: min x^2 - 2x + 0.2y + offset 1 with x^2 <= y, y binary;
// optimum (1, 1), source objective 0.2 (minlp-oa.md section 11).
inline markov_cero::model::Model case_a_source() {
    markov_cero::model::Model source;
    source.name = "minlp02_case_a";
    source.matrix = markov_cero::model::SparseMatrixBuilder(0, 2).build();
    source.objective = {-2.0, 0.2};
    source.objective_offset = 1.0;
    source.variable_name = {"x", "y"};
    source.variable_lower = {Bound::finite(0.0), Bound::finite(0.0)};
    source.variable_upper = {Bound::finite(2.0), Bound::finite(1.0)};
    source.variable_type = {VariableType::continuous, VariableType::integer};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{1.0, 0, 0, true}};
    source.nlcon_constraints = {
        {"x2_le_y", 0.0, {{1.0, 0, 0, true}, {-1.0, 1, 0, false}}}};
    source.validate();
    return source;
}

// min x0^2 + x1^2 s.t. x0 + x1 >= 1.5, x1 integer; optimum (0.5, 1), 1.25.
inline markov_cero::model::Model basic_source() {
    markov_cero::model::Model source;
    source.name = "minlp02_basic";
    markov_cero::model::SparseMatrixBuilder rows(1, 2);
    rows.add(0, 0, 1.0);
    rows.add(0, 1, 1.0);
    source.matrix = rows.build();
    source.objective = {0.0, 0.0};
    source.row_name = {"demand"};
    source.row_lower = {Bound::finite(1.5)};
    source.row_upper = {Bound::positive_infinity()};
    source.variable_name = {"x0", "x1"};
    source.variable_lower = {Bound::finite(0.0), Bound::finite(0.0)};
    source.variable_upper = {Bound::finite(3.0), Bound::finite(3.0)};
    source.variable_type = {VariableType::continuous, VariableType::integer};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{1.0, 0, 0, true}, {1.0, 1, 1, true}};
    source.validate();
    return source;
}

// Maximize -(x0^2 + x1^2) with the same rows; source objective -1.25.
inline markov_cero::model::Model maximize_source() {
    auto source = basic_source();
    source.name = "minlp02_maximize";
    source.objective_sense = ObjectiveSense::maximize;
    source.nlobj_terms = {{-1.0, 0, 0, true}, {-1.0, 1, 1, true}};
    source.validate();
    return source;
}

// x0 + x1 = 10 with x in [0, 3]^2: structurally infeasible MINLP.
inline markov_cero::model::Model infeasible_source() {
    auto source = basic_source();
    source.name = "minlp02_infeasible";
    source.row_lower = {Bound::finite(10.0)};
    source.row_upper = {Bound::finite(10.0)};
    source.validate();
    return source;
}

// Serialized proof text of an attached record (contract section 6.3 form).
inline std::string serialize(const markov_cero::verify::OaProof& proof) {
    std::ostringstream stream;
    markov_cero::verify::write_oa_proof(stream, proof);
    return stream.str();
}

} // namespace minlp02
