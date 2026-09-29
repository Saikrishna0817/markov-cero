#pragma once

// Backlog item 10 (code portion): unit-bearing refinery input/result types.
// Conversions are run-time checked against a dimension registry, so a request
// to convert across dimensions (for example wt% -> psi, RON -> cetane) is
// rejected with std::invalid_argument instead of silently rescaling.
// Synthetic/public-data qualification only; this is not a plant model.

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace markov_cero::refinery {

enum class UnitDimension {
    volume_flow,
    volume,
    mass,
    sulfur_mass_fraction,
    octane_index,
    cetane_index,
    vapor_pressure,
    price_volume,
    price_mass,
    api_gravity,
    volume_fraction,
    mass_flow
};

struct UnitDef final {
    const char* symbol;
    UnitDimension dimension;
    double to_canonical;
};

[[nodiscard]] inline const char* dimension_name(UnitDimension dimension) {
    switch (dimension) {
    case UnitDimension::volume_flow: return "volume_flow";
    case UnitDimension::volume: return "volume";
    case UnitDimension::mass: return "mass";
    case UnitDimension::sulfur_mass_fraction: return "sulfur_mass_fraction";
    case UnitDimension::octane_index: return "octane_index";
    case UnitDimension::cetane_index: return "cetane_index";
    case UnitDimension::vapor_pressure: return "vapor_pressure";
    case UnitDimension::price_volume: return "price_volume";
    case UnitDimension::price_mass: return "price_mass";
    case UnitDimension::api_gravity: return "api_gravity";
    case UnitDimension::volume_fraction: return "volume_fraction";
    case UnitDimension::mass_flow: return "mass_flow";
    }
    return "unknown";
}

// Barrel and cubic-metre factors are exact for the 42 US gallon barrel.
inline constexpr double kM3PerBbl = 0.158987294928;

[[nodiscard]] inline const std::array<UnitDef, 24>& unit_registry() {
    static const std::array<UnitDef, 24> units{{
        {"bbl/d", UnitDimension::volume_flow, kM3PerBbl},
        {"kbpd", UnitDimension::volume_flow, 1000.0 * kM3PerBbl},
        {"m3/d", UnitDimension::volume_flow, 1.0},
        {"bbl", UnitDimension::volume, kM3PerBbl},
        {"m3", UnitDimension::volume, 1.0},
        {"L", UnitDimension::volume, 1e-3},
        {"kg", UnitDimension::mass, 1.0},
        {"t", UnitDimension::mass, 1000.0},
        {"kt", UnitDimension::mass, 1e6},
        {"wt%", UnitDimension::sulfur_mass_fraction, 1e-2},
        {"ppm", UnitDimension::sulfur_mass_fraction, 1e-6},
        {"wt_frac", UnitDimension::sulfur_mass_fraction, 1.0},
        {"RON", UnitDimension::octane_index, 1.0},
        {"cetane", UnitDimension::cetane_index, 1.0},
        {"psi", UnitDimension::vapor_pressure, 6894.757293168361},
        {"kPa", UnitDimension::vapor_pressure, 1000.0},
        {"USD/bbl", UnitDimension::price_volume, 1.0 / kM3PerBbl},
        {"USD/m3", UnitDimension::price_volume, 1.0},
        {"USD/t", UnitDimension::price_mass, 1e-3},
        {"USD/kg", UnitDimension::price_mass, 1.0},
        {"degAPI", UnitDimension::api_gravity, 1.0},
        {"vol_frac", UnitDimension::volume_fraction, 1.0},
        {"vol%", UnitDimension::volume_fraction, 1e-2},
        {"t/d", UnitDimension::mass_flow, 1000.0},
    }};
    return units;
}

[[nodiscard]] inline const UnitDef& lookup_unit(std::string_view symbol) {
    for (const auto& unit : unit_registry())
        if (symbol == unit.symbol) return unit;
    throw std::invalid_argument("unknown unit: " + std::string(symbol));
}

/// Converts between two units of the same dimension; rejects unknown units and
/// dimension mismatches (for example "wt%" -> "psi" or "RON" -> "cetane").
[[nodiscard]] inline double convert(double value, std::string_view from, std::string_view to) {
    const UnitDef& source = lookup_unit(from);
    const UnitDef& target = lookup_unit(to);
    if (source.dimension != target.dimension)
        throw std::invalid_argument("unit mismatch: " + std::string(from) + " [" +
                                    dimension_name(source.dimension) + "] cannot be converted to " +
                                    std::string(to) + " [" + dimension_name(target.dimension) + "]");
    if (!std::isfinite(value)) throw std::invalid_argument("value must be finite for unit conversion");
    return value * source.to_canonical / target.to_canonical;
}

/// Unit-bearing scalar carried through inputs, reports and conversions.
class Quantity final {
  public:
    Quantity(double value, std::string_view unit) : value_(value), unit_(&lookup_unit(unit)) {
        if (!std::isfinite(value)) throw std::invalid_argument("quantity must be finite");
    }

    [[nodiscard]] double value() const noexcept { return value_; }
    [[nodiscard]] std::string_view unit() const noexcept { return unit_->symbol; }
    [[nodiscard]] UnitDimension dimension() const noexcept { return unit_->dimension; }

    [[nodiscard]] Quantity to(std::string_view target) const {
        return Quantity(convert(value_, unit_->symbol, target), target);
    }
    [[nodiscard]] double numeric_in(std::string_view target) const { return to(target).value(); }

  private:
    double value_;
    const UnitDef* unit_;
};

inline void require_finite_nonnegative(double value, const char* what, const char* unit) {
    if (!std::isfinite(value) || value < 0.0)
        throw std::invalid_argument(std::string(what) + " must be finite and >= 0 [" + unit + "]");
}

inline void require_range(double value, double low, double high, const char* what, const char* unit) {
    if (!std::isfinite(value) || value < low || value > high)
        throw std::invalid_argument(std::string(what) + " outside supported range [" + unit + "]");
}

[[nodiscard]] inline Quantity volume_flow(double value, std::string_view unit) {
    Quantity quantity(value, unit);
    if (quantity.dimension() != UnitDimension::volume_flow)
        throw std::invalid_argument("volume flow requires a volume-per-day unit");
    return quantity;
}
[[nodiscard]] inline Quantity price_per_barrel(double usd) { return {usd, "USD/bbl"}; }

// --- Density-basis conversions -------------------------------------------
// API gravity (60 F) -> approximate stock-tank density [t/m3], assuming
// reference-water density 1 t/m3 at 60 F; no thermal correction is modelled.
[[nodiscard]] inline double api_gravity_to_density_t_per_m3(double api_gravity) {
    if (!std::isfinite(api_gravity) || api_gravity <= -131.5)
        throw std::invalid_argument("API gravity outside supported range [degAPI]");
    return 141.5 / (api_gravity + 131.5);
}

// Sulfur: wt% and ppm are both mass-fraction bases, so the pair round-trips.
[[nodiscard]] inline double sulfur_wt_pct_to_ppm(double wt_pct) { return convert(wt_pct, "wt%", "ppm"); }
[[nodiscard]] inline double sulfur_ppm_to_wt_pct(double ppm) { return convert(ppm, "ppm", "wt%"); }
[[nodiscard]] inline double sulfur_ppm_to_mass_fraction(double ppm) { return convert(ppm, "ppm", "wt_frac"); }
[[nodiscard]] inline double sulfur_wt_pct_to_mass_fraction(double wt_pct) {
    return convert(wt_pct, "wt%", "wt_frac");
}

// Volumetric sulfur (mg/L) needs a declared density basis to become a ppm
// mass fraction; the density is rejected when it is not a positive finite kg/L.
[[nodiscard]] inline double sulfur_ppm_from_mg_per_l(double mg_per_l, double density_kg_per_l) {
    if (!std::isfinite(mg_per_l) || mg_per_l < 0.0)
        throw std::invalid_argument("sulfur must be finite and >= 0 [mg/L]");
    if (!std::isfinite(density_kg_per_l) || density_kg_per_l <= 0.0)
        throw std::invalid_argument("density basis must be finite and > 0 [kg/L]");
    return mg_per_l / density_kg_per_l;
}

[[nodiscard]] inline double sulfur_mg_per_l_from_ppm(double ppm, double density_kg_per_l) {
    if (!std::isfinite(ppm) || ppm < 0.0)
        throw std::invalid_argument("sulfur must be finite and >= 0 [ppm]");
    const double mass_fraction = sulfur_ppm_to_mass_fraction(ppm);
    if (!std::isfinite(density_kg_per_l) || density_kg_per_l <= 0.0)
        throw std::invalid_argument("density basis must be finite and > 0 [kg/L]");
    return mass_fraction * density_kg_per_l * 1e6;
}

// USD/bbl -> USD/t needs a stream density basis (bbl is a volume).
[[nodiscard]] inline double price_usd_per_tonne_from_bbl(double usd_per_bbl, double density_t_per_m3) {
    if (!std::isfinite(usd_per_bbl)) throw std::invalid_argument("price must be finite [USD/bbl]");
    if (!std::isfinite(density_t_per_m3) || density_t_per_m3 <= 0.0)
        throw std::invalid_argument("density basis must be finite and > 0 [t/m3]");
    return usd_per_bbl / (kM3PerBbl * density_t_per_m3);
}

[[nodiscard]] inline double kbpd_to_m3_per_day(double kbpd) { return convert(kbpd, "kbpd", "m3/d"); }

} // namespace markov_cero::refinery
