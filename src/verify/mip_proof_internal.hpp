#pragma once
#include "markov_cero/verify/mip_proof.hpp"
#include "markov_cero/model/model_snapshot.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"
#include "markov_cero/verify/primal_verifier.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace markov_cero::verify::mip_detail {
struct budget_exhausted_error : std::runtime_error {
    MipProofBudgetKind kind;
    budget_exhausted_error(MipProofBudgetKind budget, const std::string& text)
        : std::runtime_error(text), kind(budget) {}
};
struct unsupported_model_error : std::invalid_argument {
    explicit unsupported_model_error(const std::string& text) : std::invalid_argument(text) {}
};
inline std::string budget_label(MipProofBudgetKind budget) {
    switch (budget) {
    case MipProofBudgetKind::time_limit: return "time budget (deadline)";
    case MipProofBudgetKind::node_limit: return "node budget (maximum_nodes)";
    case MipProofBudgetKind::witness_limit: return "witness budget (maximum_witness_values)";
    default: return "none";
    }
}
inline bool supported_for_proof(const model::Model& model) {
    return !model.has_nlobj_section && model.nlcon_constraints.empty() &&
        !model.nlp_callbacks.has_value();
}
inline std::string bound_fingerprint(const model::Model& model) {
    return std::to_string(model::hash_model(model).fingerprint());
}

struct Domain {
    std::size_t node{};
    std::vector<model::Bound> lower, upper;
};
inline bool expired(const MipProofOptions& options) {
    return options.deadline && std::chrono::steady_clock::now() >= *options.deadline;
}
inline model::Model normalized(model::Model model) {
    if (model.has_nlobj_section || !model.nlcon_constraints.empty() || model.nlp_callbacks.has_value())
        throw std::invalid_argument("proof replay supports linear and convex quadratic MIP only");
    if (model.objective_sense == model::ObjectiveSense::maximize) {
        for (auto& c : model.objective) c = -c;
        for (auto& q : model.quadratic_matrix.value) q = -q;
        model.objective_offset = -model.objective_offset;
        model.objective_sense = model::ObjectiveSense::minimize;
    }
    return model;
}
inline bool empty(const Domain& d) {
    for (std::size_t j = 0; j < d.lower.size(); ++j)
        if (d.lower[j].is_finite() && d.upper[j].is_finite() && d.lower[j].value > d.upper[j].value)
            return true;
    return false;
}
inline std::pair<Domain, Domain> children(const Domain& parent, const MipProofNode& record) {
    auto down = parent, up = parent;
    down.node = record.down; up.node = record.up;
    auto& hi = down.upper[record.variable];
    if (!hi.is_finite() || hi.value > record.split_value) hi = model::Bound::finite(record.split_value);
    auto& lo = up.lower[record.variable];
    if (!lo.is_finite() || lo.value < record.split_value + 1) lo = model::Bound::finite(record.split_value + 1);
    return {std::move(down), std::move(up)};
}
inline double objective_sign(const model::Model& m) {
    return m.objective_sense == model::ObjectiveSense::maximize ? -1 : 1;
}
struct Relaxation { lp::reference::Result witness; std::vector<double> primal; double bound{}; };
Relaxation solve_relaxation(const model::Model&, const MipProofOptions&);
double check_relaxation(const model::Model&, const lp::reference::Result&, const MipProofOptions&);
} // namespace markov_cero::verify::mip_detail
