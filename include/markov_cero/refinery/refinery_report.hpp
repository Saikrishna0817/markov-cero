#pragma once

// Backlog item 10 (code portion): named refinery reports. Rows, active bounds
// and quality margins carry unit labels; IIS entries are scope-labelled
// (row versus variable bound) and the unsupported bound scope is stated.
// Synthetic/public-data qualification only; engineer approval (IR-34) and the
// shadow trial (G8) are NOT obtained.

#include "markov_cero/analysis/iis_analyzer.hpp"
#include "markov_cero/refinery/refinery_quality.hpp"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

namespace markov_cero::refinery {

struct ReportRow {
    std::string name;
    std::string unit;
    std::string coefficient_unit;
    std::string dual_unit;
    double activity{0.0};
    double lower_slack{0.0}; // NaN when the bound is infinite
    double upper_slack{0.0};
    double dual{0.0};
    bool binding{false};
};

struct ReportBound {
    std::string name;
    std::string variable;
    std::string unit;
    std::string side; // "lower" | "upper"
    double value{0.0};
    double reduced_cost{0.0};
    std::string reduced_cost_unit{"USD/bbl"};
};

struct ReportVariable {
    std::string name;
    std::string unit;
    double value{0.0};
    double objective_coefficient{0.0};
    std::string objective_coefficient_unit{"USD/bbl"};
};

struct IisEntry {
    std::string name;
    std::string scope; // "row" | "variable_bound"
    std::string unit;
    std::string detail;
};

struct RefineryReport {
    std::string model_name;
    std::string status;
    std::string qualification;
    std::string objective_unit{"kUSD/d"};
    double objective{0.0};
    std::vector<ReportRow> rows;
    std::vector<ReportVariable> variables;
    std::vector<ReportBound> active_bounds;
    std::vector<SpecMargin> quality_margins;
    std::vector<IisEntry> iis;
    std::string iis_scope_note;
    std::vector<std::string> unmodelled_specs;
};

[[nodiscard]] inline std::string row_unit(const std::string& row_name) {
    const std::string capacity = "_CAPACITY_MAX";
    if (row_name.size() >= capacity.size() &&
        row_name.compare(row_name.size() - capacity.size(), capacity.size(), capacity) == 0)
        return "kbpd";
    if (row_name.rfind("BAL_", 0) == 0) return "kbpd";
    if (row_name.rfind("SPEC_MS_MIN_OCTANE", 0) == 0) return "kbpd.RON";
    if (row_name.find("SULFUR") != std::string::npos) return "t/d";
    throw std::invalid_argument("unknown refinery row unit: " + row_name);
}

[[nodiscard]] inline std::string variable_unit(const std::string& variable_name) {
    if (variable_name.rfind("CRUDE_", 0) == 0 || variable_name.rfind("FLOW_", 0) == 0 ||
        variable_name.rfind("PROD_", 0) == 0)
        return "kbpd";
    throw std::invalid_argument("unknown refinery variable unit: " + variable_name);
}

[[nodiscard]] inline RefineryReport build_refinery_report(const RefineryPlanningConfig& cfg,
                                                          const model::Model& model,
                                                          const api::SolveResult& result,
                                                          const analysis::IisResult* iis = nullptr) {
    RefineryReport report;
    report.model_name = model.name;
    report.status = lp::reference::to_string(result.status);
    report.objective = result.status == lp::reference::SolveStatus::optimal
        ? (result.original_primal.size() == model.matrix.column_count
            ? result.original_objective : result.objective)
        : std::numeric_limits<double>::quiet_NaN();
    report.qualification =
        "synthetic/public-data qualification only; not a plant model; refinery engineer approval "
        "(IR-34/G34) and shadow-trial agreement (G8) NOT obtained; gates OPEN";
    report.unmodelled_specs = {
        "cetane index: unit-validated, unmodelled, rejected when requested",
        "reid vapor pressure: unit-validated, unmodelled, rejected when requested",
        "sulfur for gasoline/diesel/ATF pools: unit-validated, unmodelled, rejected when requested"};

    for (std::size_t i = 0; i < model.row_name.size(); ++i) {
        ReportRow row;
        row.name = model.row_name[i];
        row.unit = row_unit(row.name);
        row.coefficient_unit = row.unit + " per kbpd";
        row.dual_unit = "kUSD/d per " + row.unit;
        row.activity = i < result.row_activities.size() ? result.row_activities[i]
                                                        : std::numeric_limits<double>::quiet_NaN();
        row.lower_slack = i < result.row_lower_slacks.size()
            ? result.row_lower_slacks[i] : std::numeric_limits<double>::quiet_NaN();
        row.upper_slack = i < result.row_upper_slacks.size()
            ? result.row_upper_slacks[i] : std::numeric_limits<double>::quiet_NaN();
        row.dual = i < result.row_duals.size() ? result.row_duals[i]
                                               : std::numeric_limits<double>::quiet_NaN();
        const bool lower_binding = model.row_lower[i].is_finite() && std::isfinite(row.lower_slack) &&
                                   std::fabs(row.lower_slack) <= 1e-6;
        const bool upper_binding = model.row_upper[i].is_finite() && std::isfinite(row.upper_slack) &&
                                   std::fabs(row.upper_slack) <= 1e-6;
        row.binding = lower_binding || upper_binding;
        report.rows.push_back(std::move(row));
    }

    const bool has_primal =
        (result.original_primal.size() == model.matrix.column_count ||
         result.primal.size() == model.matrix.column_count) &&
        result.row_activities.size() == model.row_name.size();
    if (has_primal) {
        const std::vector<double>& x = column_values(result, model.matrix.column_count);
        for (std::size_t j = 0; j < model.variable_name.size(); ++j) {
            report.variables.push_back(ReportVariable{model.variable_name[j], variable_unit(model.variable_name[j]),
                                                       x[j], model.objective[j], "USD/bbl"});
            const auto add_bound = [&](const char* side, const model::Bound& bound) {
                if (!bound.is_finite() || std::fabs(x[j] - bound.value) > 1e-6) return;
                ReportBound active;
                active.variable = model.variable_name[j];
                active.unit = variable_unit(active.variable);
                active.side = side;
                active.name = active.variable + "." + active.side;
                active.value = bound.value;
                active.reduced_cost = j < result.reduced_costs.size() ? result.reduced_costs[j]
                    : std::numeric_limits<double>::quiet_NaN();
                report.active_bounds.push_back(std::move(active));
            };
            add_bound("lower", model.variable_lower[j]);
            add_bound("upper", model.variable_upper[j]);
        }
        report.quality_margins.push_back(gasoline_ron_margin(model, result));
        if (cfg.products[3].max_sulfur_ppm != kUnsetSulfurLimitPpm)
            report.quality_margins.push_back(fuel_oil_sulfur_margin(cfg, model, result));
        for (auto& margin : capacity_margins(model, result))
            report.quality_margins.push_back(std::move(margin));
    } else {
        for (std::size_t j = 0; j < model.variable_name.size(); ++j)
            report.variables.push_back(ReportVariable{model.variable_name[j], variable_unit(model.variable_name[j]),
                std::numeric_limits<double>::quiet_NaN(), model.objective[j], "USD/bbl"});
    }

    report.iis_scope_note =
        "IIS entries are row scope; variable bounds are held unchanged and a bound-scope "
        "(bound-minimal) IIS is unsupported by this analyzer.";
    if (iis != nullptr && iis->is_infeasible) {
        for (const auto& entry : iis->irreducible_subsystem) {
            report.iis.push_back(
                IisEntry{entry.row_name, "row", row_unit(entry.row_name), entry.description});
        }
    } else {
        report.iis_scope_note = "no IIS computed for this result; " + report.iis_scope_note;
    }
    return report;
}

} // namespace markov_cero::refinery

#include "markov_cero/refinery/refinery_report_json.hpp"
