#include "mip_proof_internal.hpp"
#include <string>
namespace markov_cero::verify {
MipProof build_mip_proof(const model::Model& source, const std::vector<double>& incumbent,
    double objective, bool infeasible, const MipProofOptions& options,
    const std::string& model_fingerprint,
    const std::vector<MipObligation>& audit_annotations) {
    using namespace mip_detail;
    if (model_fingerprint.size() > kMipProofFingerprintLimit)
        throw std::invalid_argument("model fingerprint exceeds proof format limit");
    const auto started = std::chrono::steady_clock::now();
    MipProof proof;
    proof.incumbent = incumbent; proof.objective = objective; proof.claims_infeasible = infeasible;
    proof.model_fingerprint = model_fingerprint;
    proof.obligations = audit_annotations;
    proof.format_version = kMipProofFormatVersion;
    auto model = normalized(source);
    proof.nodes.emplace_back();
    std::vector<Domain> pending{{0, model.variable_lower, model.variable_upper}};
    std::size_t values = 0;
    const double cutoff = objective_sign(source) * objective;
    while (!pending.empty() && !expired(options)) {
        auto domain = std::move(pending.back()); pending.pop_back();
        if (empty(domain)) { proof.nodes[domain.node].kind = MipProofKind::empty_domain; continue; }
        model.variable_lower = domain.lower; model.variable_upper = domain.upper;
        Relaxation leaf;
        try {
            leaf = solve_relaxation(model, options);
        } catch (const budget_exhausted_error& error) {
            proof.budget_exhausted = true;
            proof.exhausted_budget = error.kind;
            break;
        }
        auto& result = leaf.witness;
        values += result.primal.size() + result.dual.size() + result.certificate.size();
        if (values > options.maximum_witness_values) {
            proof.budget_exhausted = true;
            proof.exhausted_budget = MipProofBudgetKind::witness_limit;
            break;
        }
        auto& node = proof.nodes[domain.node];
        if (result.status == lp::reference::SolveStatus::infeasible) {
            node.kind = MipProofKind::infeasible; node.relaxation = std::move(result); continue;
        }
        if (result.status != lp::reference::SolveStatus::optimal) break;
        const double allowance = (options.tolerance + options.relative_gap) * std::max(1.0, std::abs(cutoff));
        if (!infeasible && leaf.bound >= cutoff - allowance) {
            node.kind = MipProofKind::bound; node.relaxation = std::move(result); continue;
        }
        const auto& x = leaf.primal;
        std::size_t variable = x.size();
        double best_fraction = 0;
        for (std::size_t j = 0; j < x.size(); ++j)
            if (model.variable_type[j] != model::VariableType::continuous) {
                const double f = std::abs(x[j] - std::round(x[j]));
                if (f > options.tolerance && f > best_fraction) { variable = j; best_fraction = f; }
            }
        if (variable == x.size()) break;
        if (proof.nodes.size() + 2 > options.maximum_nodes) {
            proof.budget_exhausted = true;
            proof.exhausted_budget = MipProofBudgetKind::node_limit;
            break;
        }
        node.kind = MipProofKind::split; node.variable = variable; node.split_value = std::floor(x[variable]);
        node.down = proof.nodes.size(); node.up = node.down + 1;
        auto [down, up] = children(domain, node);
        proof.nodes.resize(proof.nodes.size() + 2);
        pending.push_back(std::move(up)); pending.push_back(std::move(down));
    }
    if (!proof.budget_exhausted && expired(options) &&
        (!pending.empty() || std::any_of(proof.nodes.begin(), proof.nodes.end(),
            [](const MipProofNode& node) { return node.kind == MipProofKind::open; }))) {
        proof.budget_exhausted = true;
        proof.exhausted_budget = MipProofBudgetKind::time_limit;
    }
    proof.nodes_used = proof.nodes.size();
    proof.witness_values_used = values;
    proof.budget_time_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    return proof;
}
} // namespace markov_cero::verify
