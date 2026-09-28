#include "markov_cero/refinery/refinery_model.hpp"

namespace markov_cero::refinery {
RefineryPlanningConfig make_synthetic_config() {
    RefineryPlanningConfig cfg;
    cfg.refinery_name = "Synthetic_Volume_Planning";
    cfg.cdu_capacity_kbpd = 300.0;
    cfg.ccr_capacity_kbpd = 50.0;
    cfg.fcc_capacity_kbpd = 70.0;
    cfg.dhdt_capacity_kbpd = 80.0;

    // Arab Light Crude
    CrudeAssay al;
    al.name = "Synthetic_Light";
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
    ah.name = "Synthetic_Heavy";
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
    ms.product_name = "Synthetic_Gasoline";
    ms.min_demand_kbpd = 30.0;
    ms.max_demand_kbpd = 80.0;
    ms.price_per_barrel = 95.0;
    ms.min_ron = 91.0;

    cfg.products.push_back(ms);

    ProductSpecification hsd;
    hsd.product_name = "Synthetic_Diesel";
    hsd.min_demand_kbpd = 50.0;
    hsd.max_demand_kbpd = 120.0;
    hsd.price_per_barrel = 92.0;


    cfg.products.push_back(hsd);

    ProductSpecification atf;
    atf.product_name = "Aviation_Turbine_Fuel";
    atf.min_demand_kbpd = 20.0;
    atf.max_demand_kbpd = 45.0;
    atf.price_per_barrel = 98.0;

    cfg.products.push_back(atf);

    ProductSpecification fo;
    fo.product_name = "Fuel_Oil_Residue";
    fo.min_demand_kbpd = 20.0;
    fo.max_demand_kbpd = 90.0;
    fo.price_per_barrel = 55.0;
    cfg.products.push_back(fo);

    return cfg;
}

RefineryPlanningConfig make_standard_mrpl_config() { return make_synthetic_config(); }

} // namespace markov_cero::refinery
