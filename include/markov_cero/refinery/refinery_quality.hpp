#pragma once

// Backlog item 10 (code portion): unit-checked quality inputs and results.
// Margins are reported as slack-to-limit in the unit of the limit itself, and
// the FCC feed balance (IR-13 extension) is checked in a converted common unit.
// Synthetic/public-data qualification only; engineer review (IR-34/G8) open.

#include "markov_cero/api/solve.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/refinery/refinery_model.hpp"
#include "markov_cero/refinery/refinery_units.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace markov_cero::refinery {

/// Slack-to-limit of one configured specification, in the limit's own unit.
struct SpecMargin {
    std::string spec;
    std::string unit;
    std::string sense{"max"}; // "max": margin = limit - value; "min": value - limit
    double limit{0.0};
    double value{0.0};
    double margin{0.0};
    bool satisfied{false};
};

/// Unit-checked FCC feed balance (extension of IR-13): FCC gasoline output
/// divided by the conversion factor recovered from FCC_CAPACITY_MAX must stay
/// within the gas-oil feed available from the crudes.
struct FeedBalanceCheck {
    bool satisfied{false};
    double fcc_feed{0.0};
    double available_gas_oil{0.0};
    double slack{0.0};
    std::string unit{"kbpd"};
    std::string detail;
};

[[nodiscard]] inline FeedBalanceCheck compare_fcc_feed(const Quantity& feed, const Quantity& supply) {
    const double feed_bbl_per_day = feed.numeric_in("bbl/d");
    const double supply_bbl_per_day = supply.numeric_in("bbl/d");
    FeedBalanceCheck check;
    check.fcc_feed = feed.numeric_in("kbpd");
    check.available_gas_oil = supply.numeric_in("kbpd");
    check.slack = Quantity(supply_bbl_per_day - feed_bbl_per_day, "bbl/d").numeric_in("kbpd");
    check.satisfied = check.slack >= -1e-6;
    check.detail = "FCC feed " + std::to_string(check.fcc_feed) + " kbpd <= available gas oil " +
                   std::to_string(check.available_gas_oil) + " kbpd";
    return check;
}

[[nodiscard]] inline const std::vector<double>& column_values(const api::SolveResult& result,
                                                              std::size_t columns) {
    if (result.original_primal.size() == columns) return result.original_primal;
    if (result.primal.size() == columns) return result.primal;
    throw std::invalid_argument("solve result carries no original-space primal vector");
}

[[nodiscard]] inline std::size_t column_of(const model::Model& model, const std::string& name) {
    for (std::size_t j = 0; j < model.variable_name.size(); ++j)
        if (model.variable_name[j] == name) return j;
    throw std::invalid_argument("unknown refinery column: " + name);
}

[[nodiscard]] inline std::size_t row_of(const model::Model& model, const std::string& name) {
    for (std::size_t i = 0; i < model.row_name.size(); ++i)
        if (model.row_name[i] == name) return i;
    throw std::invalid_argument("unknown refinery row: " + name);
}

[[nodiscard]] inline double coefficient_of(const model::Model& model, std::size_t row, std::size_t column) {
    for (std::size_t k = model.matrix.column_start[column]; k < model.matrix.column_start[column + 1]; ++k)
        if (model.matrix.row_index[k] == row) return model.matrix.value[k];
    return 0.0;
}

[[nodiscard]] inline double row_activity(const api::SolveResult& result, const model::Model& model,
                                         std::size_t row) {
    if (row < result.row_activities.size()) return result.row_activities[row];
    const std::vector<double> activities =
        model.matrix.multiply(column_values(result, model.matrix.column_count));
    if (row >= activities.size()) throw std::invalid_argument("row index out of range");
    return activities[row];
}

/// FCC feed balance in kbpd, converted through the unit registry on both sides.
[[nodiscard]] inline FeedBalanceCheck check_fcc_feed_balance(const model::Model& model,
                                                             const api::SolveResult& result) {
    const std::vector<double>& x = column_values(result, model.matrix.column_count);
    const std::size_t fcc_out = column_of(model, "FLOW_FCC_TO_MS");
    const std::size_t cap_row = row_of(model, "FCC_CAPACITY_MAX");
    const std::size_t gas_oil_row = row_of(model, "BAL_LGO");
    const double feed_factor = coefficient_of(model, cap_row, fcc_out); // 1 / conversion
    if (!std::isfinite(feed_factor) || feed_factor <= 0.0)
        throw std::invalid_argument("FCC capacity row does not carry a positive feed factor");

    const Quantity feed = volume_flow(x[fcc_out] * feed_factor, "kbpd");
    double available = 0.0;
    for (std::size_t c = 0; c < model.matrix.column_count; ++c) {
        const double yield = coefficient_of(model, gas_oil_row, c);
        if (c != fcc_out && yield > 0.0 && c < x.size()) available += yield * x[c];
    }
    const Quantity supply = volume_flow(available, "kbpd");
    return compare_fcc_feed(feed, supply);
}

/// Gasoline octane margin [RON], derived from the SPEC_MS_MIN_OCTANE_RON row.
[[nodiscard]] inline SpecMargin gasoline_ron_margin(const model::Model& model, const api::SolveResult& result) {
    const std::size_t row = row_of(model, "SPEC_MS_MIN_OCTANE_RON");
    const std::size_t gasoline = column_of(model, "PROD_MOTOR_SPIRIT");
    const double limit = -coefficient_of(model, row, gasoline);
    const std::vector<double>& x = column_values(result, model.matrix.column_count);
    const double volume = x[gasoline];
    SpecMargin margin;
    margin.spec = "gasoline_blend_min_ron";
    margin.unit = "RON";
    margin.sense = "min";
    margin.limit = limit;
    margin.value = volume > 1e-9 ? (row_activity(result, model, row) + limit * volume) / volume : limit;
    margin.margin = margin.value - limit;
    margin.satisfied = row_activity(result, model, row) >= -1e-6;
    return margin;
}

/// Fuel-oil sulfur margin [ppm] over the residue pool (common-draw blend).
[[nodiscard]] inline SpecMargin fuel_oil_sulfur_margin(const RefineryPlanningConfig& cfg,
                                                       const model::Model& model,
                                                       const api::SolveResult& result) {
    const std::vector<double>& x = column_values(result, model.matrix.column_count);
    const double volume_per_kbpd = kbpd_to_m3_per_day(1.0);
    double sulfur_mass = 0.0;
    double pool_mass = 0.0;
    for (std::size_t c = 0; c < cfg.crudes.size() && c < x.size(); ++c) {
        const double mass = cfg.crudes[c].residue_yield * x[c] * volume_per_kbpd *
                            api_gravity_to_density_t_per_m3(cfg.crudes[c].api_gravity);
        pool_mass += mass;
        sulfur_mass += mass * sulfur_wt_pct_to_mass_fraction(cfg.crudes[c].sulfur_wt_pct);
    }
    SpecMargin margin;
    margin.spec = "fuel_oil_max_sulfur_ppm";
    margin.unit = "ppm";
    margin.sense = "max";
    margin.limit = cfg.products[3].max_sulfur_ppm;
    margin.value = pool_mass > 1e-12 ? (sulfur_mass / pool_mass) * 1e6 : 0.0;
    margin.margin = margin.limit - margin.value;
    margin.satisfied = margin.value <= margin.limit + 1e-6;
    return margin;
}

/// Capacity slack-to-limit [kbpd] for every *_CAPACITY_MAX row present.
[[nodiscard]] inline std::vector<SpecMargin> capacity_margins(const model::Model& model,
                                                              const api::SolveResult& result) {
    std::vector<SpecMargin> margins;
    for (std::size_t i = 0; i < model.row_name.size(); ++i) {
        const std::string& name = model.row_name[i];
        constexpr std::size_t suffix_size = sizeof("_CAPACITY_MAX") - 1;
        if (name.size() < suffix_size ||
            name.compare(name.size() - suffix_size, suffix_size, "_CAPACITY_MAX") != 0) continue;
        if (!model.row_upper[i].is_finite()) continue;
        SpecMargin margin;
        margin.spec = name;
        margin.unit = "kbpd";
        margin.sense = "max";
        margin.limit = model.row_upper[i].value;
        margin.value = row_activity(result, model, i);
        margin.margin = margin.limit - margin.value;
        margin.satisfied = margin.value <= margin.limit + 1e-6;
        margins.push_back(std::move(margin));
    }
    return margins;
}

} // namespace markov_cero::refinery
