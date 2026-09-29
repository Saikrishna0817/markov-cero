#pragma once

#include "markov_cero/refinery/refinery_model.hpp"
#include "markov_cero/refinery/refinery_units.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace markov_cero::refinery {

/// The unit of every numeric field in the legacy double-based planning input.
/// Typed setters below convert compatible quantities and reject all mismatches.
[[nodiscard]] inline std::string_view input_unit(std::string_view scope, std::string_view field) {
    if (scope == "refinery") {
        if (field == "cdu_capacity_kbpd" || field == "ccr_capacity_kbpd" ||
            field == "fcc_capacity_kbpd" || field == "dhdt_capacity_kbpd") return "kbpd";
    } else if (scope == "crude") {
        if (field == "cost_per_barrel") return "USD/bbl";
        if (field == "max_availability_kbpd") return "kbpd";
        if (field == "api_gravity") return "degAPI";
        if (field == "sulfur_wt_pct") return "wt%";
        if (field == "lpg_yield" || field == "light_naphtha_yield" ||
            field == "heavy_naphtha_yield" || field == "kerosene_yield" ||
            field == "gas_oil_yield" || field == "residue_yield") return "vol_frac";
    } else if (scope == "product") {
        if (field == "min_demand_kbpd" || field == "max_demand_kbpd") return "kbpd";
        if (field == "price_per_barrel") return "USD/bbl";
        if (field == "min_ron") return "RON";
        if (field == "max_sulfur_ppm") return "ppm";
        if (field == "min_cetane") return "cetane";
        if (field == "max_rvp_psi") return "psi";
    }
    throw std::invalid_argument("unknown refinery input field: " + std::string(scope) + "." +
                                std::string(field));
}

inline void set_input(RefineryPlanningConfig& config, std::string_view field, const Quantity& value) {
    double* target = nullptr;
    if (field == "cdu_capacity_kbpd") target = &config.cdu_capacity_kbpd;
    else if (field == "ccr_capacity_kbpd") target = &config.ccr_capacity_kbpd;
    else if (field == "fcc_capacity_kbpd") target = &config.fcc_capacity_kbpd;
    else if (field == "dhdt_capacity_kbpd") target = &config.dhdt_capacity_kbpd;
    if (!target) throw std::invalid_argument("unknown refinery capacity input: " + std::string(field));
    *target = value.numeric_in(input_unit("refinery", field));
}

inline void set_input(CrudeAssay& crude, std::string_view field, const Quantity& value) {
    double* target = nullptr;
    if (field == "cost_per_barrel") target = &crude.cost_per_barrel;
    else if (field == "max_availability_kbpd") target = &crude.max_availability_kbpd;
    else if (field == "api_gravity") target = &crude.api_gravity;
    else if (field == "sulfur_wt_pct") target = &crude.sulfur_wt_pct;
    else if (field == "lpg_yield") target = &crude.lpg_yield;
    else if (field == "light_naphtha_yield") target = &crude.light_naphtha_yield;
    else if (field == "heavy_naphtha_yield") target = &crude.heavy_naphtha_yield;
    else if (field == "kerosene_yield") target = &crude.kerosene_yield;
    else if (field == "gas_oil_yield") target = &crude.gas_oil_yield;
    else if (field == "residue_yield") target = &crude.residue_yield;
    if (!target) throw std::invalid_argument("unknown crude input: " + std::string(field));
    *target = value.numeric_in(input_unit("crude", field));
}

inline void set_input(ProductSpecification& product, std::string_view field, const Quantity& value) {
    double* target = nullptr;
    if (field == "min_demand_kbpd") target = &product.min_demand_kbpd;
    else if (field == "max_demand_kbpd") target = &product.max_demand_kbpd;
    else if (field == "price_per_barrel") target = &product.price_per_barrel;
    else if (field == "min_ron") target = &product.min_ron;
    else if (field == "max_sulfur_ppm") target = &product.max_sulfur_ppm;
    else if (field == "min_cetane") target = &product.min_cetane;
    else if (field == "max_rvp_psi") target = &product.max_rvp_psi;
    if (!target) throw std::invalid_argument("unknown product input: " + std::string(field));
    *target = value.numeric_in(input_unit("product", field));
}

} // namespace markov_cero::refinery
