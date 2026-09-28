#include "markov_cero/refinery/refinery_model.hpp"
#include <cmath>
#include <stdexcept>

namespace markov_cero::refinery {

model::Model build_refinery_lp(const RefineryPlanningConfig& cfg) {
    if (cfg.crudes.empty() || cfg.products.size() != 4)
        throw std::invalid_argument("synthetic refinery requires crudes and exactly four ordered products");
    auto nonnegative = [](double x) { return std::isfinite(x) && x >= 0; };
    for (double capacity : {cfg.cdu_capacity_kbpd, cfg.ccr_capacity_kbpd,
                            cfg.fcc_capacity_kbpd, cfg.dhdt_capacity_kbpd})
        if (!nonnegative(capacity)) throw std::invalid_argument("invalid unit capacity");
    for (const auto& crude : cfg.crudes) {
        if (!std::isfinite(crude.cost_per_barrel) || !nonnegative(crude.max_availability_kbpd))
            throw std::invalid_argument("invalid crude price or availability");
        double sum = 0;
        for (double yield : {crude.lpg_yield, crude.light_naphtha_yield, crude.heavy_naphtha_yield,
                             crude.kerosene_yield, crude.gas_oil_yield, crude.residue_yield}) {
            if (!nonnegative(yield)) throw std::invalid_argument("invalid crude yield");
            sum += yield;
        }
        if (sum > 1.0 + 1e-9) throw std::invalid_argument("synthetic volume yields exceed unity");
    }
    for (const auto& product : cfg.products) {
        if (!nonnegative(product.min_demand_kbpd) || !nonnegative(product.max_demand_kbpd) ||
            product.min_demand_kbpd > product.max_demand_kbpd || !std::isfinite(product.price_per_barrel) ||
            !nonnegative(product.min_ron)) throw std::invalid_argument("invalid product demand, price or RON");
        if (product.max_sulfur_ppm != 1e9 || product.min_cetane != 0 || product.max_rvp_psi != 100)
            throw std::invalid_argument("synthetic model cannot enforce sulfur, cetane or RVP; supply a qualified process model");
    }
    model::Model model;
    model.name = cfg.refinery_name;
    model.objective_sense = model::ObjectiveSense::minimize;
    model.objective_offset = 0.0;

    // Decision variables:
    // 0..num_crudes-1: Crude intake rates F_c
    // Followed by intermediate unit processing flows and final product streams
    const std::size_t num_crudes = cfg.crudes.size();

    // Variables mapping:
    // c: Crude intake F_c (0..num_crudes-1)
    // LN_to_MS, Reformate_to_MS, FCC_to_MS
    // Kero_to_ATF, Kero_to_HSD
    // LGO_to_DHDT, DHDT_to_HSD
    // Residue_to_FO
    const std::size_t col_LN_to_MS = num_crudes;
    const std::size_t col_Reformate_to_MS = num_crudes + 1;
    const std::size_t col_FCC_to_MS = num_crudes + 2;
    const std::size_t col_Kero_to_ATF = num_crudes + 3;
    const std::size_t col_Kero_to_HSD = num_crudes + 4;
    const std::size_t col_LGO_to_DHDT = num_crudes + 5;
    const std::size_t col_DHDT_to_HSD = num_crudes + 6;
    const std::size_t col_Residue_to_FO = num_crudes + 7;
    const std::size_t col_Prod_MS = num_crudes + 8;
    const std::size_t col_Prod_HSD = num_crudes + 9;
    const std::size_t col_Prod_ATF = num_crudes + 10;
    const std::size_t col_Prod_FO = num_crudes + 11;
    const std::size_t total_cols = num_crudes + 12;

    model.variable_name.resize(total_cols);
    model.variable_lower.resize(total_cols, model::Bound::finite(0.0));
    model.variable_upper.resize(total_cols, model::Bound::positive_infinity());
    model.variable_type.resize(total_cols, model::VariableType::continuous);
    model.objective.resize(total_cols, 0.0);

    // Crude bounds and costs
    for (std::size_t c = 0; c < num_crudes; ++c) {
        model.variable_name[c] = "CRUDE_" + cfg.crudes[c].name;
        model.variable_upper[c] = model::Bound::finite(cfg.crudes[c].max_availability_kbpd);
        model.objective[c] = cfg.crudes[c].cost_per_barrel; // Cost to minimize
    }

    model.variable_name[col_LN_to_MS] = "FLOW_LN_TO_MS";
    model.variable_name[col_Reformate_to_MS] = "FLOW_REFORMATE_TO_MS";
    model.variable_name[col_FCC_to_MS] = "FLOW_FCC_TO_MS";
    model.variable_name[col_Kero_to_ATF] = "FLOW_KERO_TO_ATF";
    model.variable_name[col_Kero_to_HSD] = "FLOW_KERO_TO_HSD";
    model.variable_name[col_LGO_to_DHDT] = "FLOW_LGO_TO_DHDT";
    model.variable_name[col_DHDT_to_HSD] = "FLOW_DHDT_TO_HSD";
    model.variable_name[col_Residue_to_FO] = "FLOW_RESIDUE_TO_FO";

    model.variable_name[col_Prod_MS] = "PROD_MOTOR_SPIRIT";
    model.variable_name[col_Prod_HSD] = "PROD_HIGH_SPEED_DIESEL";
    model.variable_name[col_Prod_ATF] = "PROD_AVIATION_TURBINE_FUEL";
    model.variable_name[col_Prod_FO] = "PROD_FUEL_OIL";

    // Operating costs for conversion units
    model.objective[col_Reformate_to_MS] = 4.0; // Reformer processing cost
    model.objective[col_FCC_to_MS] = 3.5;       // FCC processing cost
    model.objective[col_LGO_to_DHDT] = 2.0;      // Hydrotreater cost

    // Product revenues (negative objective coefficients)
    model.objective[col_Prod_MS] = -cfg.products[0].price_per_barrel;
    model.objective[col_Prod_HSD] = -cfg.products[1].price_per_barrel;
    model.objective[col_Prod_ATF] = -cfg.products[2].price_per_barrel;
    model.objective[col_Prod_FO] = -cfg.products[3].price_per_barrel;

    // Product demand bounds
    model.variable_lower[col_Prod_MS] = model::Bound::finite(cfg.products[0].min_demand_kbpd);
    model.variable_upper[col_Prod_MS] = model::Bound::finite(cfg.products[0].max_demand_kbpd);

    model.variable_lower[col_Prod_HSD] = model::Bound::finite(cfg.products[1].min_demand_kbpd);
    model.variable_upper[col_Prod_HSD] = model::Bound::finite(cfg.products[1].max_demand_kbpd);

    model.variable_lower[col_Prod_ATF] = model::Bound::finite(cfg.products[2].min_demand_kbpd);
    model.variable_upper[col_Prod_ATF] = model::Bound::finite(cfg.products[2].max_demand_kbpd);

    model.variable_lower[col_Prod_FO] = model::Bound::finite(cfg.products[3].min_demand_kbpd);
    model.variable_upper[col_Prod_FO] = model::Bound::finite(cfg.products[3].max_demand_kbpd);

    // Build constraints
    model::SparseMatrixBuilder builder(15, total_cols);
    std::size_t row_idx = 0;

    auto add_row = [&](const std::string& name, model::Bound lb, model::Bound ub) {
        model.row_name.push_back(name);
        model.row_lower.push_back(lb);
        model.row_upper.push_back(ub);
        return row_idx++;
    };

    // 1. CDU Capacity: sum F_c <= cdu_capacity
    {
        const auto r = add_row("CDU_CAPACITY_MAX", model::Bound::negative_infinity(),
                               model::Bound::finite(cfg.cdu_capacity_kbpd));
        for (std::size_t c = 0; c < num_crudes; ++c) builder.add(r, c, 1.0);
    }

    // 2. Light Naphtha balance: LN_to_MS <= sum (yield_LN_c * F_c)
    {
        const auto r = add_row("BAL_LIGHT_NAPHTHA", model::Bound::finite(0.0),
                               model::Bound::positive_infinity());
        for (std::size_t c = 0; c < num_crudes; ++c) {
            builder.add(r, c, cfg.crudes[c].light_naphtha_yield);
        }
        builder.add(r, col_LN_to_MS, -1.0);
    }

    // 3. CCR / Reformer balance & capacity: Reformate <= sum (yield_HN_c * F_c) * 0.85
    {
        const auto r = add_row("BAL_REFORMER", model::Bound::finite(0.0),
                               model::Bound::positive_infinity());
        for (std::size_t c = 0; c < num_crudes; ++c) {
            builder.add(r, c, cfg.crudes[c].heavy_naphtha_yield * 0.85);
        }
        builder.add(r, col_Reformate_to_MS, -1.0);

        const auto r_cap = add_row("CCR_CAPACITY_MAX", model::Bound::negative_infinity(),
                                   model::Bound::finite(cfg.ccr_capacity_kbpd));
        builder.add(r_cap, col_Reformate_to_MS, 1.0 / 0.85);
    }

    // 4. FCC balance & capacity
    {
        const auto r_cap = add_row("FCC_CAPACITY_MAX", model::Bound::negative_infinity(),
                                   model::Bound::finite(cfg.fcc_capacity_kbpd));
        builder.add(r_cap, col_FCC_to_MS, 1.0 / 0.60);
    }

    // 5. Kerosene balance: Kero_to_ATF + Kero_to_HSD <= sum (yield_Kero_c * F_c)
    {
        const auto r = add_row("BAL_KEROSENE", model::Bound::finite(0.0),
                               model::Bound::positive_infinity());
        for (std::size_t c = 0; c < num_crudes; ++c) {
            builder.add(r, c, cfg.crudes[c].kerosene_yield);
        }
        builder.add(r, col_Kero_to_ATF, -1.0);
        builder.add(r, col_Kero_to_HSD, -1.0);
    }

    // 6. LGO / Hydrotreater balance & capacity
    {
        const auto r = add_row("BAL_LGO", model::Bound::finite(0.0),
                               model::Bound::positive_infinity());
        for (std::size_t c = 0; c < num_crudes; ++c) {
            builder.add(r, c, cfg.crudes[c].gas_oil_yield);
        }
        builder.add(r, col_LGO_to_DHDT, -1.0);
        builder.add(r, col_FCC_to_MS, -1.0 / 0.60); // Shared gas-oil feed; no free FCC output.

        const auto r_dhdt = add_row("DHDT_CAPACITY_MAX", model::Bound::negative_infinity(),
                                    model::Bound::finite(cfg.dhdt_capacity_kbpd));
        builder.add(r_dhdt, col_LGO_to_DHDT, 1.0);

        const auto r_yield = add_row("BAL_DHDT_YIELD", model::Bound::finite(0.0),
                                     model::Bound::finite(0.0));
        builder.add(r_yield, col_LGO_to_DHDT, 0.98); // 98% hydrotreated liquid yield
        builder.add(r_yield, col_DHDT_to_HSD, -1.0);
    }

    // 7. Residue balance: Residue_to_FO <= sum (yield_Residue_c * F_c)
    {
        const auto r = add_row("BAL_RESIDUE", model::Bound::finite(0.0),
                               model::Bound::positive_infinity());
        for (std::size_t c = 0; c < num_crudes; ++c) {
            builder.add(r, c, cfg.crudes[c].residue_yield);
        }
        builder.add(r, col_Residue_to_FO, -1.0);
    }

    // 8. Finished Gasoline (MS) Volume Balance: LN + Reformate + FCC = MS
    {
        const auto r = add_row("BAL_VOL_MS", model::Bound::finite(0.0), model::Bound::finite(0.0));
        builder.add(r, col_LN_to_MS, 1.0);
        builder.add(r, col_Reformate_to_MS, 1.0);
        builder.add(r, col_FCC_to_MS, 1.0);
        builder.add(r, col_Prod_MS, -1.0);
    }

    // 9. Gasoline Octane Quality: 65*LN + 100*Reformate + 92*FCC >= 91*MS
    // => 65*LN + 100*Reformate + 92*FCC - 91*MS >= 0
    {
        const auto r = add_row("SPEC_MS_MIN_OCTANE_RON", model::Bound::finite(0.0),
                               model::Bound::positive_infinity());
        builder.add(r, col_LN_to_MS, 65.0);
        builder.add(r, col_Reformate_to_MS, 100.0);
        builder.add(r, col_FCC_to_MS, 92.0);
        builder.add(r, col_Prod_MS, -cfg.products[0].min_ron);
    }

    // 10. Finished Diesel (HSD) Volume Balance: Clean_Diesel + Kero = HSD
    {
        const auto r = add_row("BAL_VOL_HSD", model::Bound::finite(0.0), model::Bound::finite(0.0));
        builder.add(r, col_DHDT_to_HSD, 1.0);
        builder.add(r, col_Kero_to_HSD, 1.0);
        builder.add(r, col_Prod_HSD, -1.0);
    }

    // 11. Finished ATF Volume: Kero = ATF
    {
        const auto r = add_row("BAL_VOL_ATF", model::Bound::finite(0.0), model::Bound::finite(0.0));
        builder.add(r, col_Kero_to_ATF, 1.0);
        builder.add(r, col_Prod_ATF, -1.0);
    }

    // 12. Finished Fuel Oil Volume: Residue = FO
    {
        const auto r = add_row("BAL_VOL_FO", model::Bound::finite(0.0), model::Bound::finite(0.0));
        builder.add(r, col_Residue_to_FO, 1.0);
        builder.add(r, col_Prod_FO, -1.0);
    }

    model.matrix = builder.build();
    model.validate();
    return model;
}

} // namespace markov_cero::refinery
