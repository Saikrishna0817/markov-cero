#include "markov_cero/model/model_snapshot.hpp"

#include <string>
#include <utility>

namespace markov_cero::model {
namespace {

std::size_t csc_bytes(const SparseMatrixCSC& matrix) noexcept {
    // M5: 64-bit index + 64-bit value entries, plus the column pointer array.
    return 16U * matrix.value.size() + 8U * (matrix.column_count + 1U);
}

std::size_t bound_bytes(std::size_t count) noexcept {
    return 2U * sizeof(Bound) * count;
}

std::size_t name_bytes(const std::vector<std::string>& names) noexcept {
    std::size_t bytes = 0;
    for (const std::string& name : names) bytes += sizeof(std::string) + name.capacity();
    return bytes;
}

std::size_t poly_bytes(std::size_t count) noexcept {
    return sizeof(NlobjTerm) * count;
}

std::size_t callbacks_bytes(const nlp::NlpModel& callbacks) noexcept {
    std::size_t bytes = sizeof(nlp::NlpModel) + callbacks.name.capacity();
    bytes += 8U * (callbacks.lower_bounds.size() + callbacks.upper_bounds.size());
    bytes += sizeof(nlp::NlpModel::PolyTerm) * callbacks.poly_terms.size();
    bytes += 8U * callbacks.linear_objective.size();
    return bytes;
}

} // namespace

ModelSnapshot ModelSnapshot::capture(const Model& source) {
    Model copy = source;
    copy.validate();
    const ModelHashes hashes = hash_model(copy);
    return ModelSnapshot(std::move(copy), hashes);
}

ModelSnapshot ModelSnapshot::capture(Model&& source) {
    source.validate();
    const ModelHashes hashes = hash_model(source);
    return ModelSnapshot(std::move(source), hashes);
}

std::size_t ModelSnapshot::estimated_bytes() const noexcept {
    const Model& value = model_;
    std::size_t bytes = csc_bytes(value.matrix);
    bytes += 8U * value.objective.size();
    bytes += bound_bytes(value.variable_lower.size());
    bytes += bound_bytes(value.row_lower.size());
    bytes += sizeof(VariableType) * value.variable_type.size();
    bytes += name_bytes(value.row_name);
    bytes += name_bytes(value.variable_name);
    if (value.has_quadratic_objective) bytes += csc_bytes(value.quadratic_matrix);
    bytes += poly_bytes(value.nlobj_terms.size());
    for (const NlconConstraint& constraint : value.nlcon_constraints) {
        bytes += sizeof(NlconConstraint) + constraint.name.capacity();
        bytes += poly_bytes(constraint.terms.size());
    }
    if (value.nlp_callbacks) bytes += callbacks_bytes(*value.nlp_callbacks);
    return bytes;
}

} // namespace markov_cero::model
