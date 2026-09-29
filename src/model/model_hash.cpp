#include "markov_cero/model/model_snapshot.hpp"

#include <bit>
#include <string>

namespace markov_cero::model {
namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

/// FNV-1a over explicit byte streams so structural and numeric content stay
/// separable for cache invalidation (a value change must not look structural).
class Hasher final {
  public:
    void byte(std::uint8_t value) {
        hash_ ^= static_cast<std::uint64_t>(value);
        hash_ *= kFnvPrime;
    }
    void u64(std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8)
            byte(static_cast<std::uint8_t>(value >> shift));
    }
    void f64(double value) { u64(std::bit_cast<std::uint64_t>(value)); }
    void flag(bool value) { byte(value ? 1U : 0U); }
    void text(const std::string& value) {
        u64(value.size());
        for (const char character : value) byte(static_cast<std::uint8_t>(character));
    }
    [[nodiscard]] std::uint64_t finish() const noexcept { return hash_; }

  private:
    std::uint64_t hash_{kFnvOffset};
};

void hash_bound_kind(Hasher& hasher, const Bound& bound) {
    hasher.u64(static_cast<std::uint64_t>(bound.kind));
}

void hash_bound_value(Hasher& hasher, const Bound& bound) {
    if (bound.kind == BoundKind::finite) hasher.f64(bound.value);
}

void hash_structure(Hasher& hasher, const SparseMatrixCSC& matrix) {
    hasher.u64(matrix.row_count);
    hasher.u64(matrix.column_count);
    for (const std::size_t pointer : matrix.column_start) hasher.u64(pointer);
    for (const std::size_t index : matrix.row_index) hasher.u64(index);
}

void hash_values(Hasher& hasher, const SparseMatrixCSC& matrix) {
    for (const double value : matrix.value) hasher.f64(value);
}

void hash_poly_structure(Hasher& hasher, const NlobjTerm& term) {
    hasher.u64(term.var0);
    hasher.u64(term.var1);
    hasher.flag(term.quadratic);
}

void hash_model_structure(Hasher& hasher, const Model& model) {
    hasher.text(model.name);
    hasher.u64(static_cast<std::uint64_t>(model.objective_sense));
    hash_structure(hasher, model.matrix);
    hasher.u64(model.objective.size());
    for (const VariableType type : model.variable_type)
        hasher.u64(static_cast<std::uint64_t>(type));
    for (const Bound& bound : model.variable_lower) hash_bound_kind(hasher, bound);
    for (const Bound& bound : model.variable_upper) hash_bound_kind(hasher, bound);
    for (const Bound& bound : model.row_lower) hash_bound_kind(hasher, bound);
    for (const Bound& bound : model.row_upper) hash_bound_kind(hasher, bound);
    for (const std::string& name : model.row_name) hasher.text(name);
    for (const std::string& name : model.variable_name) hasher.text(name);
    hasher.flag(model.has_quadratic_objective);
    if (model.has_quadratic_objective) hash_structure(hasher, model.quadratic_matrix);
    hasher.flag(model.has_nlobj_section);
    hasher.u64(model.nlobj_terms.size());
    for (const NlobjTerm& term : model.nlobj_terms) hash_poly_structure(hasher, term);
    hasher.u64(model.nlcon_constraints.size());
    for (const NlconConstraint& constraint : model.nlcon_constraints) {
        hasher.text(constraint.name);
        hasher.u64(constraint.terms.size());
        for (const NlobjTerm& term : constraint.terms) hash_poly_structure(hasher, term);
    }
    hasher.flag(model.nlp_callbacks.has_value());
    if (model.nlp_callbacks) {
        const nlp::NlpModel& callbacks = *model.nlp_callbacks;
        hasher.text(callbacks.name);
        hasher.u64(callbacks.n_vars);
        hasher.u64(callbacks.n_ineq);
        hasher.u64(callbacks.n_eq);
        hasher.flag(callbacks.has_callbacks());
        hasher.flag(callbacks.from_nlobj);
        hasher.u64(callbacks.poly_terms.size());
        for (const nlp::NlpModel::PolyTerm& term : callbacks.poly_terms) {
            hasher.u64(term.var0);
            hasher.u64(term.var1);
            hasher.flag(term.quadratic);
        }
    }
}

void hash_model_numeric(Hasher& hasher, const Model& model) {
    hasher.f64(model.objective_offset);
    for (const double value : model.objective) hasher.f64(value);
    for (const Bound& bound : model.variable_lower) hash_bound_value(hasher, bound);
    for (const Bound& bound : model.variable_upper) hash_bound_value(hasher, bound);
    for (const Bound& bound : model.row_lower) hash_bound_value(hasher, bound);
    for (const Bound& bound : model.row_upper) hash_bound_value(hasher, bound);
    hash_values(hasher, model.matrix);
    if (model.has_quadratic_objective) hash_values(hasher, model.quadratic_matrix);
    for (const NlobjTerm& term : model.nlobj_terms) hasher.f64(term.coefficient);
    for (const NlconConstraint& constraint : model.nlcon_constraints) {
        hasher.f64(constraint.rhs);
        for (const NlobjTerm& term : constraint.terms) hasher.f64(term.coefficient);
    }
    if (model.nlp_callbacks) {
        const nlp::NlpModel& callbacks = *model.nlp_callbacks;
        for (const double bound : callbacks.lower_bounds) hasher.f64(bound);
        for (const double bound : callbacks.upper_bounds) hasher.f64(bound);
        for (const nlp::NlpModel::PolyTerm& term : callbacks.poly_terms)
            hasher.f64(term.coefficient);
        for (const double value : callbacks.linear_objective) hasher.f64(value);
        hasher.f64(callbacks.objective_offset);
    }
}

/// splitmix64-style mixing so the combined identity is not a linear reuse of
/// either component (equal fingerprints require equal content of both kinds).
std::uint64_t mix(std::uint64_t left, std::uint64_t right) noexcept {
    std::uint64_t value = left + 0x9e3779b97f4a7c15ULL + right;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

} // namespace

std::uint64_t ModelHashes::fingerprint() const noexcept { return mix(structural, numeric); }

ModelHashes hash_model(const Model& model) noexcept {
    Hasher structural;
    hash_model_structure(structural, model);
    Hasher numeric;
    hash_model_numeric(numeric, model);
    return {structural.finish(), numeric.finish()};
}

} // namespace markov_cero::model
