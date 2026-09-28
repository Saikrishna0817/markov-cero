#include "markov_cero/refinery/refinery_model.hpp"

namespace markov_cero::refinery {

RefineryPlanningConfig make_standard_mrpl_config() {
    RefineryPlanningConfig cfg;
    cfg.refinery_name = "MRPL_Phase3_Configuration";
    cfg.cdu_capacity_kbpd = 300.0;
    cfg.ccr_capacity_kbpd = 50.0;
    cfg.fcc_capacity_kbpd = 70.0;
    cfg.dhdt_capacity_kbpd = 80.0;

    // Arab Light Crude
    CrudeAssay al;
    al.name = "Arab_Light";
    al.cost_per_barrel = 75.0;
    al.max_availability_kbpd = 200.0;
    al.api_gravity = 33.4;
    al.sulfur_wt_pct = 1.77;
    al.lpg_yield = 0.03;
    al.light_naphtha_yield = 0.08;
    al.heavy_naphtha_yield = 0.14;
    al.kerosene_yield = 0.15;
    al.gas_oil_yield = 0.28;
    al.residue_yield = 0.32;
    cfg.crudes.push_back(al);

    // Arab Heavy Crude
    CrudeAssay ah;
    ah.name = "Arab_Heavy";
    ah.cost_per_barrel = 68.0;
    ah.max_availability_kbpd = 180.0;
    ah.api_gravity = 27.9;
    ah.sulfur_wt_pct = 2.85;
    ah.lpg_yield = 0.02;
    ah.light_naphtha_yield = 0.05;
    ah.heavy_naphtha_yield = 0.10;
    ah.kerosene_yield = 0.12;
    ah.gas_oil_yield = 0.25;
    ah.residue_yield = 0.46;
    cfg.crudes.push_back(ah);

    // Finished Products
    ProductSpecification ms;
    ms.product_name = "Motor_Spirit_BS_VI";
    ms.min_demand_kbpd = 30.0;
    ms.max_demand_kbpd = 80.0;
    ms.price_per_barrel = 95.0;
    ms.min_ron = 91.0;
    ms.max_sulfur_ppm = 10.0;
    cfg.products.push_back(ms);

    ProductSpecification hsd;
    hsd.product_name = "High_Speed_Diesel_BS_VI";
    hsd.min_demand_kbpd = 50.0;
    hsd.max_demand_kbpd = 120.0;
    hsd.price_per_barrel = 92.0;
    hsd.min_cetane = 51.0;
    hsd.max_sulfur_ppm = 10.0;
    cfg.products.push_back(hsd);

    ProductSpecification atf;
    atf.product_name = "Aviation_Turbine_Fuel";
    atf.min_demand_kbpd = 20.0;
    atf.max_demand_kbpd = 45.0;
    atf.price_per_barrel = 98.0;
    atf.max_sulfur_ppm = 15.0;
    cfg.products.push_back(atf);

    ProductSpecification fo;
    fo.product_name = "Fuel_Oil_Residue";
    fo.min_demand_kbpd = 20.0;
    fo.max_demand_kbpd = 90.0;
    fo.price_per_barrel = 55.0;
    cfg.products.push_back(fo);

    return cfg;
}

model::Model build_refinery_lp(const RefineryPlanningConfig& cfg) {
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
