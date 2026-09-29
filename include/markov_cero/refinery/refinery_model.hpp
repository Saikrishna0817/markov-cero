#pragma once

#include "markov_cero/model/model.hpp"

#include <string>
#include <vector>

namespace markov_cero::refinery {

/// Sentinel "no sulfur limit requested" for ProductSpecification::max_sulfur_ppm.
/// Any other value is enforced (product 3) or rejected as unmodelled.
inline constexpr double kUnsetSulfurLimitPpm = 1e9;

struct CrudeAssay {
    std::string name;
    double cost_per_barrel{0.0};       // USD / bbl
    double max_availability_kbpd{0.0};  // Thousand barrels per day (kbpd)
    double api_gravity{0.0};
    double sulfur_wt_pct{0.0};

    // Distillation yields (volume fraction summing to ~1.0)
    double lpg_yield{0.0};
    double light_naphtha_yield{0.0};
    double heavy_naphtha_yield{0.0};
    double kerosene_yield{0.0};
    double gas_oil_yield{0.0};
    double residue_yield{0.0};
};

struct ProductSpecification {
    std::string product_name;
    double min_demand_kbpd{0.0};
    double max_demand_kbpd{0.0};
    double price_per_barrel{0.0};       // USD / bbl

    // Quality specs (checked by the typed input setters and build_refinery_lp;
    // genuinely unmodelled specs are rejected).
    double min_ron{0.0};                        // Research Octane Number [RON], gasoline pool only
    double max_sulfur_ppm{kUnsetSulfurLimitPpm}; // Sulfur limit [ppm, mass basis], fuel-oil pool only
    double min_cetane{0.0};                     // Cetane number [cetane index], unmodelled
    double max_rvp_psi{100.0};                  // Reid Vapor Pressure [psi], unmodelled
};

struct RefineryPlanningConfig {
    std::string refinery_name{"Synthetic_Volume_Planning"};
    double cdu_capacity_kbpd{300.0};   // CDU throughput limit
    double ccr_capacity_kbpd{50.0};    // Reformer capacity
    double fcc_capacity_kbpd{70.0};    // FCC capacity
    double dhdt_capacity_kbpd{80.0};   // Hydrotreater capacity

    std::vector<CrudeAssay> crudes;
    std::vector<ProductSpecification> products;
};

/// Synthetic volume-planning demonstration, not a plant mass/quality model.
/// Gasoline RON and fuel-oil sulfur are modelled as synthetic blend proxies.
/// Other quality limits are rejected.
[[nodiscard]] model::Model build_refinery_lp(const RefineryPlanningConfig& config);

/// Generates explicitly synthetic demonstration data.
[[nodiscard]] RefineryPlanningConfig make_synthetic_config();

/// Legacy compatibility name; returns synthetic data, never MRPL operating data.
[[nodiscard]] RefineryPlanningConfig make_standard_mrpl_config();

} // namespace markov_cero::refinery
