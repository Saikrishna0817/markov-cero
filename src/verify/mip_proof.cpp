#include "mip_proof_internal.hpp"
#include <string>
namespace markov_cero::verify {
MipProofReport verify_mip_proof(const model::Model& source, const MipProof& proof,
                               const MipProofOptions& options) {
    using namespace mip_detail;
    MipProofReport report;
    const auto started = std::chrono::steady_clock::now();
    report.format_version = proof.format_version;
    report.model_fingerprint = proof.model_fingerprint;
    report.nodes_used = proof.nodes_used;
    report.witness_values_used = proof.witness_values_used;
    report.budget_time_ms = proof.budget_time_ms;
    report.build_ms = proof.budget_time_ms;
    try {
        source.validate();
        if (!supported_for_proof(source))
            throw unsupported_model_error(
                "unsupported model class for proof replay: linear and convex quadratic MIP only");
        if (!(options.tolerance > 0 && options.tolerance <= 1e-4) ||
            !std::isfinite(options.relative_gap) || options.relative_gap < 0)
            throw std::invalid_argument("invalid proof tolerance or gap");
        if (proof.format_version != kMipProofFormatVersion)
            throw std::invalid_argument("unsupported MIP proof format version " +
                std::to_string(proof.format_version) + " (expected " +
                std::to_string(kMipProofFormatVersion) + ")");
        if (!proof.model_fingerprint.empty() && proof.model_fingerprint != bound_fingerprint(source))
            throw std::invalid_argument("proof model fingerprint does not match the verified model");
        // Contract §7.1: obligations stay audit annotations (the tree replay
        // is cut-free), but a tampered artifact must not carry structurally
        // invalid rows — wrong domain or non-finite values — past replay.
        for (const auto& obligation : proof.obligations) {
            if (obligation.kind == MipObligationKind::cut) {
                bool finite_row = std::isfinite(obligation.rhs) &&
                                  std::isfinite(obligation.observed_lhs);
                for (double coefficient : obligation.coefficients)
                    finite_row = finite_row && std::isfinite(coefficient);
                if (obligation.coefficients.size() != source.matrix.column_count || !finite_row)
                    throw std::invalid_argument(
                        "invalid cut obligation row (out-of-domain or non-finite)");
            } else if (obligation.source_row >= source.matrix.row_count ||
                       obligation.variable >= source.matrix.column_count ||
                       !std::isfinite(obligation.source_coefficient) ||
                       !std::isfinite(obligation.source_rhs) ||
                       !std::isfinite(obligation.derived_bound)) {
                throw std::invalid_argument(
                    "invalid propagation obligation (out-of-domain or non-finite)");
            }
        }
        if (proof.budget_exhausted)
            throw budget_exhausted_error(proof.exhausted_budget,
                "proof build exhausted " + budget_label(proof.exhausted_budget));
        if (expired(options))
            throw budget_exhausted_error(MipProofBudgetKind::time_limit,
                "proof replay exhausted " + budget_label(MipProofBudgetKind::time_limit));
        auto model = normalized(source);
        if (proof.nodes.empty()) throw std::invalid_argument("proof node budget or missing root");
        if (proof.nodes.size() > options.maximum_nodes)
            throw budget_exhausted_error(MipProofBudgetKind::node_limit,
                "proof replay exhausted " + budget_label(MipProofBudgetKind::node_limit));
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
            if (expired(options))
                throw budget_exhausted_error(MipProofBudgetKind::time_limit,
                    "proof replay exhausted " + budget_label(MipProofBudgetKind::time_limit));
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
            report.checked_witness_values = values;
            if (values > options.maximum_witness_values)
                throw budget_exhausted_error(MipProofBudgetKind::witness_limit,
                    "proof replay exhausted " + budget_label(MipProofBudgetKind::witness_limit));
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
        if (expired(options))
            throw budget_exhausted_error(MipProofBudgetKind::time_limit,
                "proof replay exhausted " + budget_label(MipProofBudgetKind::time_limit));
        report.accepted = true;
        report.status = MipProofStatus::accepted;
        report.tier = proof.model_fingerprint.empty() ? MipAssuranceTier::replayed_tree
                                                      : MipAssuranceTier::independent_tree;
        report.lower_bound = global_bound;
        report.message = "cut-free integer partitions and every leaf witness independently replayed "
                         "(numerical tolerances)";
        if (!proof.obligations.empty())
            report.message += "; optimizer obligations are audit annotations, not replay checks";
        if (report.tier == MipAssuranceTier::replayed_tree) report.message += " (model fingerprint unbound)";
    } catch (const budget_exhausted_error& error) {
        report.status = MipProofStatus::exhausted;
        report.budget_exhausted = true;
        report.exhausted_budget = error.kind;
        report.message = error.what();
    } catch (const unsupported_model_error& error) {
        report.status = MipProofStatus::unsupported;
        report.message = error.what();
    } catch (const std::exception& error) {
        report.status = MipProofStatus::rejected;
        report.message = error.what();
    }
    report.replay_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    return report;
}
} // namespace markov_cero::verify
