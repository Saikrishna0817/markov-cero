#include "mip_proof_internal.hpp"
namespace markov_cero::verify {
MipProof build_mip_proof(const model::Model& source, const std::vector<double>& incumbent,
    double objective, bool infeasible, const MipProofOptions& options) {
    using namespace mip_detail;
    MipProof proof;
    proof.incumbent = incumbent; proof.objective = objective; proof.claims_infeasible = infeasible;
    auto model = normalized(source);
    proof.nodes.emplace_back();
    std::vector<Domain> pending{{0, model.variable_lower, model.variable_upper}};
    std::size_t values = 0;
    const double cutoff = objective_sign(source) * objective;
    while (!pending.empty() && !expired(options)) {
        auto domain = std::move(pending.back()); pending.pop_back();
        if (empty(domain)) { proof.nodes[domain.node].kind = MipProofKind::empty_domain; continue; }
        model.variable_lower = domain.lower; model.variable_upper = domain.upper;
        auto leaf = solve_relaxation(model, options);
        auto& result = leaf.witness;
        values += result.primal.size() + result.dual.size() + result.certificate.size();
        if (values > options.maximum_witness_values) break;
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
        if (variable == x.size() || proof.nodes.size() + 2 > options.maximum_nodes) break;
        node.kind = MipProofKind::split; node.variable = variable; node.split_value = std::floor(x[variable]);
        node.down = proof.nodes.size(); node.up = node.down + 1;
        auto [down, up] = children(domain, node);
        proof.nodes.resize(proof.nodes.size() + 2);
        pending.push_back(std::move(up)); pending.push_back(std::move(down));
    }
    return proof;
}
} // namespace markov_cero::verify
