#include "pdlp_internal.hpp"
namespace markov_cero::lp::first_order::detail_pdlp {
PdlpResult solve_degenerate(const model::Model& model, const PdlpOptions& options) {
    model.validate();
    PdlpResult out;
    out.tolerance = options.primal_tolerance;
    out.objective = model.objective_offset;
    out.primal.resize(model.matrix.column_count);
    out.dual.assign(model.matrix.row_count, 0);
    out.status = PdlpStatus::numerical_failure;
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        out.status = PdlpStatus::resource_limit; out.message = "PDLP deadline reached"; return out;
    }
    if (model.matrix.column_count == 0) {
        for (std::size_t i=0; i<model.matrix.row_count; ++i) {
            if ((model.row_lower[i].is_finite() && model.row_lower[i].value > 0) ||
                (model.row_upper[i].is_finite() && model.row_upper[i].value < 0)) {
                out.message = "infeasible empty row; use primal engine for a Farkas certificate";
                return out;
            }
        }
    } else {
        const double sign = model.objective_sense == model::ObjectiveSense::maximize ? -1 : 1;
        for (std::size_t j=0; j<out.primal.size(); ++j) {
            const double cost = sign * model.objective[j];
            if (cost == 0) out.primal[j] = project_bound(0, model.variable_lower[j], model.variable_upper[j]);
            else {
                const auto bound = cost > 0 ? model.variable_lower[j] : model.variable_upper[j];
                if (!bound.is_finite()) {
                    out.message = "unbounded box LP; use primal engine for a ray certificate";
                    return out;
                }
                out.primal[j] = bound.value;
            }
            out.objective += model.objective[j] * out.primal[j];
        }
    }
    out.status = PdlpStatus::optimal;
    out.message = "empty-row or box-only LP solved analytically";
    return out;
}
}
