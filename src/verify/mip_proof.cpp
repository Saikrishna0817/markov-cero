#include "mip_proof_internal.hpp"
namespace markov_cero::verify {
MipProofReport verify_mip_proof(const model::Model& source, const MipProof& proof,
                               const MipProofOptions& options) {
    using namespace mip_detail;
    MipProofReport report;
    try {
        source.validate();
        if (!(options.tolerance > 0 && options.tolerance <= 1e-4) ||
            !std::isfinite(options.relative_gap) || options.relative_gap < 0)
            throw std::invalid_argument("invalid proof tolerance or gap");
        auto model = normalized(source);
        if (proof.nodes.empty() || proof.nodes.size() > options.maximum_nodes)
            throw std::invalid_argument("proof node budget or missing root");
        if (!proof.claims_infeasible && !verify_primal(source, {proof.incumbent, proof.objective},
                {options.tolerance, options.tolerance}, {options.tolerance, options.tolerance},
                options.tolerance).passed)
            throw std::invalid_argument("invalid proof incumbent");
        const double incumbent = objective_sign(source) * proof.objective;
        double global_bound = std::numeric_limits<double>::infinity();
        std::vector<bool> seen(proof.nodes.size());
        std::size_t values = 0;
        std::vector<Domain> pending{{0, source.variable_lower, source.variable_upper}};
        while (!pending.empty()) {
            if (expired(options)) throw std::runtime_error("proof verification deadline");
            auto domain = std::move(pending.back()); pending.pop_back();
            if (domain.node >= proof.nodes.size() || seen[domain.node])
                throw std::invalid_argument("duplicate, cyclic or missing proof node");
            seen[domain.node] = true; ++report.checked_nodes;
            const auto& node = proof.nodes[domain.node];
            if (node.kind == MipProofKind::empty_domain) {
                if (!empty(domain)) throw std::invalid_argument("false empty domain leaf");
                continue;
            }
            if (empty(domain)) throw std::invalid_argument("invalid nonempty node domain");
            if (node.kind == MipProofKind::split) {
                if (node.variable >= model.variable_type.size() ||
                    model.variable_type[node.variable] == model::VariableType::continuous ||
                    !std::isfinite(node.split_value) || std::abs(node.split_value) >= 0x1p52 ||
                    std::floor(node.split_value) != node.split_value || node.down == node.up)
                    throw std::invalid_argument("invalid integer partition");
                auto [down, up] = children(domain, node);
                pending.push_back(std::move(up)); pending.push_back(std::move(down));
                continue;
            }
            if (node.kind != MipProofKind::bound && node.kind != MipProofKind::infeasible)
                throw std::invalid_argument("unresolved proof leaf");
            values += node.relaxation.primal.size() + node.relaxation.dual.size() + node.relaxation.certificate.size();
            if (values > options.maximum_witness_values) throw std::length_error("proof witness budget");
            model.variable_lower = domain.lower; model.variable_upper = domain.upper;
            const auto expected = node.kind == MipProofKind::bound
                ? lp::reference::SolveStatus::optimal : lp::reference::SolveStatus::infeasible;
            if (node.relaxation.status != expected)
                throw std::invalid_argument("invalid leaf relaxation status");
            const double bound = check_relaxation(model, node.relaxation, options);
            if (node.kind == MipProofKind::infeasible) continue;
            if (proof.claims_infeasible) throw std::invalid_argument("feasible bound leaf cannot prove infeasibility");
            const double allowance = options.tolerance * std::max(1.0, std::abs(incumbent));
            if (!std::isfinite(bound) || bound < incumbent - allowance -
                options.relative_gap * std::max(1.0, std::abs(incumbent)))
                throw std::invalid_argument("leaf bound does not close requested gap");
            global_bound = std::min(global_bound, static_cast<double>(bound));
        }
        if (std::find(seen.begin(), seen.end(), false) != seen.end())
            throw std::invalid_argument("unreachable proof records");
        report.accepted = true;
        report.lower_bound = global_bound;
        report.message = "cut-free integer partitions and every leaf witness independently replayed (numerical tolerances)";
    } catch (const std::exception& error) { report.message = error.what(); }
    return report;
}
} // namespace markov_cero::verify
