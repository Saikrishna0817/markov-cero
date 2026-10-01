// MINLP-01 (docs/contracts/minlp-oa.md §4-§5, §7.6): OA row accumulation with
// provenance, the fail-closed in-loop replay, and the shared incumbent-
// retaining exit helper. Rows are created from the NLP callback view and
// independently replayed from the source polynomial before the master sees
// them.
#include "minlp_solver_internal.hpp"

namespace markov_cero::minlp {
namespace detail_minlp_solver {

std::string append_oa_rows(const model::Model& source, const NlpModel& nlp,
                           const std::vector<OaRowSource>& row_map,
                           const std::vector<double>& p, double f_at_p,
                           OaRowAccumulators& acc, std::size_t& cuts_replayed) {
    const bool maximize = source.objective_sense == model::ObjectiveSense::maximize;
    const std::size_t first_new = acc.cuts.size();

    // Objective cut (creation path: NLP callbacks).
    std::vector<double> grad_f = nlp.eval_gradient(p);
    if (grad_f.size() != nlp.n_vars || p.size() != nlp.n_vars) {
        return "minlp: objective gradient dimension mismatch at an expansion point";
    }
    double obj_dot = 0.0;
    for (std::size_t j = 0; j < nlp.n_vars; ++j) obj_dot += grad_f[j] * p[j];
    {
        const double exact = obj_dot - f_at_p;
        const double weakening = kOaCutWeakening * (1.0 + std::abs(exact));
        OaCut cut;
        cut.source_kind = OaCutSource::objective;
        cut.source_index = kOaObjectiveSource;
        cut.source_maximize = maximize;
        cut.point = p;
        cut.gradient = grad_f;
        cut.value = f_at_p;
        cut.weakening = weakening;
        cut.rhs = exact + weakening;
        acc.obj_grads.push_back(std::move(grad_f));
        acc.obj_rhs.push_back(exact + weakening);
        acc.cuts.push_back(std::move(cut));
    }

    // Constraint cuts; provenance from the source row map (contract §5.1).
    std::size_t n_ineq = 0, n_eq = 0;
    const auto cvals = constraint_values(nlp, p, n_ineq, n_eq);
    const auto J = constraint_jacobian(nlp, p, n_ineq, n_eq);
    if (row_map.size() != n_ineq) {
        return "minlp: source row map does not match the NLP inequality rows";
    }
    for (std::size_t i = 0; i < n_ineq; ++i) {
        if (J[i].size() != nlp.n_vars || cvals.size() != n_ineq) {
            return "minlp: constraint Jacobian dimension mismatch at an expansion point";
        }
        double row_dot = 0.0;
        for (std::size_t j = 0; j < nlp.n_vars; ++j) row_dot += J[i][j] * p[j];
        const double exact = row_dot - cvals[i];
        const double weakening = kOaCutWeakening * (1.0 + std::abs(exact));
        OaCut cut;
        cut.source_kind = row_map[i].kind;
        cut.source_index = row_map[i].index;
        cut.source_maximize = maximize;
        cut.point = p;
        cut.gradient = J[i];
        cut.value = cvals[i];
        cut.weakening = weakening;
        cut.rhs = exact + weakening;
        acc.cut_grads.push_back(J[i]);
        acc.cut_rhs.push_back(exact + weakening);
        acc.cuts.push_back(std::move(cut));
    }

    // Contract §5.2: replay every new cut against the source polynomial;
    // any mismatch fails closed before the master sees the rows.
    const std::vector<OaCut> fresh(acc.cuts.begin() + static_cast<std::ptrdiff_t>(first_new),
                                   acc.cuts.end());
    const OaReplayReport report = replay_oa_cuts(source, fresh);
    cuts_replayed += report.checked;
    if (!report.accepted) {
        return "minlp: OA cut replay from the source polynomial rejected a generated tangent: " +
               report.message;
    }
    return {};
}

void retain_incumbent(MinlpSolution& out, const std::vector<double>& best_x, double best_obj,
                      double best_bound) {
    if (!out.integer_feasible) return;
    out.x = best_x;
    out.objective = best_obj;
    if (std::isfinite(best_bound)) {
        out.best_bound = best_bound;
        out.relative_gap = std::max(0.0, best_obj - best_bound) /
                           std::max(1.0, std::abs(best_obj));
    }
}

} // namespace detail_minlp_solver
} // namespace markov_cero::minlp
