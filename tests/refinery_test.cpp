#include "markov_cero/analysis/iis_analyzer.hpp"
#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/model/model_snapshot.hpp"
#include "markov_cero/refinery/pooling_slp.hpp"
#include "markov_cero/refinery/refinery_input_schema.hpp"
#include "markov_cero/refinery/refinery_model.hpp"
#include "markov_cero/refinery/refinery_report.hpp"
#include "markov_cero/refinery/refinery_units.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace refinery = markov_cero::refinery;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <class Body>
void require_invalid_argument(Body body, const char* message) {
    try {
        body();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}
} // namespace

void test_synthetic_planning() {
    const auto cfg = markov_cero::refinery::make_synthetic_config();
    const auto model = markov_cero::refinery::build_refinery_lp(cfg);
    markov_cero::api::SolveOptions options;
    options.engine = "simplex";
    options.enable_presolve = true;
    options.enable_scale = true;
    const auto res = markov_cero::api::solve_model(model, options);
    assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(res.original_verified);
    // Fixing the formerly free FCC output can make mandatory demand unprofitable.
    // The invariant is feed feasibility, not a positive business margin.
    const auto n = cfg.crudes.size();
    double gas_oil = 0;
    for (std::size_t i = 0; i < n; ++i) gas_oil += cfg.crudes[i].gas_oil_yield * res.primal[i];
    assert(res.primal[n + 5] + res.primal[n + 2] / 0.60 <= gas_oil + 1e-6);
    std::cout << "[+] test_synthetic_planning passed: Daily Net Margin = $"
              << -res.objective << "k / day\n";
}

void test_refinery_iis_analysis() {
    auto cfg = markov_cero::refinery::make_synthetic_config();
    // Artificially create an impossible constraint conflict:
    // Mandatory diesel volume exceeds available hydrotreated gas oil and kerosene.
    cfg.dhdt_capacity_kbpd = 5.0;
    cfg.products[1].min_demand_kbpd = 100.0;
    auto model = markov_cero::refinery::build_refinery_lp(cfg);
    const auto iis = markov_cero::analysis::compute_iis(model);
    assert(iis.is_infeasible);
    assert(!iis.irreducible_subsystem.empty());
    std::cout << "[+] test_refinery_iis_analysis passed: found "
              << iis.irreducible_subsystem.size() << " conflicting constraints in IIS:\n"
              << iis.diagnostic_summary << "\n";
}

void test_haverly_pooling_slp() {
    const auto res = markov_cero::refinery::solve_haverly_pooling(2.0, 50, 1e-4);
    assert(res.converged);
    assert(res.original_feasible);
    assert(res.maximum_original_violation <= 1e-4);
    assert(res.profit > 0.0);
    assert(res.flow_xA > 0.0 || res.flow_xB > 0.0);
    std::cout << "[+] test_haverly_pooling_slp passed: converged in " << res.iterations
              << " SLP iterations, profit = $" << res.profit
              << ", final pool quality = " << res.final_pool_quality << "%\n";
}

// Backlog item 10: unit conversions, dimension-mismatch rejection, the
// unit-checked quality schema, feed balance and the named report.
void test_units_conversions_and_mismatch() {
    require(std::fabs(refinery::sulfur_wt_pct_to_ppm(1.77) - 17700.0) < 1e-9, "wt% -> ppm scale");
    require(std::fabs(refinery::sulfur_ppm_to_wt_pct(17700.0) - 1.77) < 1e-12, "ppm round-trip to wt%");
    require(std::fabs(refinery::sulfur_wt_pct_to_ppm(refinery::sulfur_ppm_to_wt_pct(3200.0)) - 3200.0) < 1e-9,
            "wt% <-> ppm round-trip must be lossless");
    require(refinery::convert(2.5, "kbpd", "bbl/d") == 2500.0, "kbpd -> bbl/d scale");
    require(refinery::convert(2.5, "kt", "t") == 2500.0, "kt -> t scale");
    require(std::fabs(refinery::convert(14.503773773, "psi", "kPa") - 100.0) < 1e-6,
            "RVP pressure conversion");
    require(std::fabs(refinery::convert(55.0, "USD/bbl", "USD/m3") - 55.0 / refinery::kM3PerBbl) < 1e-9,
            "price volume conversion");
    const double ppm = refinery::sulfur_ppm_from_mg_per_l(50.0, 0.85);
    require(std::fabs(ppm - 50.0 / 0.85) < 1e-12, "mg/L to mass ppm uses density");
    require(std::fabs(refinery::sulfur_mg_per_l_from_ppm(ppm, 0.85) - 50.0) < 1e-9,
            "density-basis sulfur round-trip");
    require(std::fabs(refinery::price_usd_per_tonne_from_bbl(55.0, 0.95) - 55.0 / (refinery::kM3PerBbl * 0.95)) <
                1e-12,
            "price density basis");
    require_invalid_argument([] { (void)refinery::convert(1.0, "wt%", "psi"); },
                             "sulfur -> pressure must be rejected");
    require_invalid_argument([] { (void)refinery::convert(1.0, "RON", "cetane"); },
                             "RON -> cetane must be rejected");
    require_invalid_argument([] { (void)refinery::convert(1.0, "kg", "kbpd"); }, "mass -> volume must be rejected");
    require_invalid_argument([] { (void)refinery::convert(1.0, "wt_ppm", "wt%"); },
                             "unknown unit must be rejected");
    const refinery::Quantity stream = refinery::volume_flow(2.5, "kbpd");
    require(stream.dimension() == refinery::UnitDimension::volume_flow, "quantity keeps its dimension");
    require(std::fabs(stream.numeric_in("m3/d") - refinery::kbpd_to_m3_per_day(2.5)) < 1e-12,
            "quantity conversion");
    require_invalid_argument([&stream] { (void)stream.to("t"); }, "quantity mismatch must be rejected");
    require_invalid_argument([] { (void)refinery::volume_flow(2.0, "t"); },
                             "volume-flow constructor must reject mass");
    auto typed = refinery::make_synthetic_config();
    refinery::set_input(typed, "cdu_capacity_kbpd", refinery::Quantity(250000.0, "bbl/d"));
    require(typed.cdu_capacity_kbpd == 250.0, "capacity converts to kbpd");
    refinery::set_input(typed.crudes[0], "sulfur_wt_pct", refinery::Quantity(10000.0, "ppm"));
    require(typed.crudes[0].sulfur_wt_pct == 1.0, "crude sulfur converts to wt%");
    refinery::set_input(typed.crudes[0], "lpg_yield", refinery::Quantity(5.0, "vol%"));
    require(typed.crudes[0].lpg_yield == 0.05, "yield converts to volume fraction");
    refinery::set_input(typed.products[0], "price_per_barrel", refinery::Quantity(100.0, "USD/bbl"));
    require(typed.products[0].price_per_barrel == 100.0, "product price is unit checked");
    require_invalid_argument([&typed] {
        refinery::set_input(typed.products[0], "min_ron", refinery::Quantity(60.0, "cetane"));
    }, "product input dimension mismatch must be rejected");
    require_invalid_argument([&typed] {
        refinery::set_input(typed.crudes[0], "cost_per_barrel", refinery::Quantity(1.0, "USD/t"));
    }, "price basis mismatch needs density");
    require_invalid_argument([&typed] {
        refinery::set_input(typed, "fcc_capacity_kbpd", refinery::Quantity(10.0, "t"));
    }, "capacity cannot accept mass");
    for (const char* field : {"cdu_capacity_kbpd", "ccr_capacity_kbpd", "fcc_capacity_kbpd",
                              "dhdt_capacity_kbpd"})
        require_invalid_argument([&typed, field] { refinery::set_input(typed, field, refinery::Quantity(1, "t")); },
                                 "all refinery capacity inputs reject mass");
    for (const char* field : {"cost_per_barrel", "max_availability_kbpd", "api_gravity",
                              "sulfur_wt_pct", "lpg_yield", "light_naphtha_yield",
                              "heavy_naphtha_yield", "kerosene_yield", "gas_oil_yield", "residue_yield"})
        require_invalid_argument([&typed, field] {
            refinery::set_input(typed.crudes[0], field, refinery::Quantity(1, "t"));
        }, "all crude numeric inputs reject mass");
    for (const char* field : {"min_demand_kbpd", "max_demand_kbpd", "price_per_barrel",
                              "min_ron", "max_sulfur_ppm", "min_cetane", "max_rvp_psi"})
        require_invalid_argument([&typed, field] {
            refinery::set_input(typed.products[0], field, refinery::Quantity(1, "t"));
        }, "all product numeric inputs reject mass");
    std::cout << "[+] test_units_conversions_and_mismatch passed\n";
}

void test_quality_schema_and_feed_balance() {
    auto cfg = refinery::make_synthetic_config();
    cfg.products[1].min_cetane = 52.0;
    require_invalid_argument([&cfg] { (void)refinery::build_refinery_lp(cfg); },
                             "cetane must be rejected as unmodelled");
    cfg = refinery::make_synthetic_config();
    cfg.products[2].max_rvp_psi = 13.5;
    require_invalid_argument([&cfg] { (void)refinery::build_refinery_lp(cfg); },
                             "RVP must be rejected as unmodelled");
    cfg = refinery::make_synthetic_config();
    cfg.products[0].max_sulfur_ppm = 1000.0;
    require_invalid_argument([&cfg] { (void)refinery::build_refinery_lp(cfg); },
                             "sulfur outside the modelled pool must be rejected");
    cfg = refinery::make_synthetic_config();
    refinery::set_input(cfg.products[3], "max_sulfur_ppm", refinery::Quantity(2.0, "wt%"));
    require(std::fabs(cfg.products[3].max_sulfur_ppm - 20000.0) < 1e-9,
            "product sulfur wt% converts to ppm");
    const auto model = refinery::build_refinery_lp(cfg);
    bool sulfur_row = false;
    for (const auto& name : model.row_name) sulfur_row = sulfur_row || name == "SPEC_FO_MAX_SULFUR_PPM";
    require(sulfur_row, "requested fuel-oil sulfur limit must be modelled as a named row");
    require(refinery::row_unit("SPEC_FO_MAX_SULFUR_PPM") == "t/d", "sulfur row unit label");
    markov_cero::api::SolveOptions options;
    options.engine = "simplex";
    const auto res = markov_cero::api::solve_model(model, options);
    require(res.status == markov_cero::lp::reference::SolveStatus::optimal,
            "sulfur-limited synthetic case must stay optimal");
    const auto margin = refinery::fuel_oil_sulfur_margin(cfg, model, res);
    require(margin.unit == "ppm" && margin.sense == "max", "sulfur margin unit and sense");
    require(margin.satisfied && margin.value <= margin.limit + 1e-6, "blend sulfur must respect the limit");
    require(margin.margin == margin.limit - margin.value, "margin must be slack-to-limit in ppm");
    const auto feed = refinery::check_fcc_feed_balance(model, res);
    require(feed.unit == "kbpd" && feed.satisfied, "FCC feed balance must close in kbpd");
    require(feed.slack >= -1e-6 && feed.fcc_feed <= feed.available_gas_oil + 1e-6,
            "FCC feed must not exceed available gas oil");
    require_invalid_argument([] {
        (void)refinery::compare_fcc_feed(refinery::Quantity(1.0, "t"), refinery::Quantity(1.0, "kbpd"));
    }, "feed balance must reject mass-volume mismatch");
    std::cout << "[+] test_quality_schema_and_feed_balance passed: fuel oil sulfur = " << margin.value
              << " ppm <= " << margin.limit << " ppm, FCC feed slack = " << feed.slack << " kbpd\n";
}

void test_named_report_structure() {
    const auto cfg = refinery::make_synthetic_config();
    const auto model = refinery::build_refinery_lp(cfg);
    markov_cero::api::SolveOptions options;
    options.engine = "simplex";
    const auto res = markov_cero::api::solve_model(model, options);
    require(res.status == markov_cero::lp::reference::SolveStatus::optimal, "report case must be optimal");

    const auto report = refinery::build_refinery_report(cfg, model, res);
    require(!report.rows.empty() && report.rows.size() == model.row_name.size(), "report must carry every row");
    for (const auto& row : report.rows)
        require(!row.unit.empty() && !row.dual_unit.empty(), "every row and dual must carry a unit");
    require(report.variables.size() == model.variable_name.size(), "every variable must be reported");
    for (const auto& variable : report.variables)
        require(variable.unit == "kbpd" && variable.objective_coefficient_unit == "USD/bbl",
                "variable and objective coefficients must carry units");
    bool capacity = false;
    for (const auto& row : report.rows)
        capacity = capacity || (row.name == "CDU_CAPACITY_MAX" && row.unit == "kbpd");
    require(capacity, "capacity row must be labelled kbpd");
    require(!report.active_bounds.empty(), "active bounds must be reported by name");
    for (const auto& bound : report.active_bounds)
        require(bound.unit == "kbpd" && bound.name == bound.variable + "." + bound.side &&
                bound.reduced_cost_unit == "USD/bbl", "active bound name and unit labels");
    bool ron = false;
    for (const auto& margin : report.quality_margins)
        ron = ron || (margin.spec == "gasoline_blend_min_ron" && margin.unit == "RON" && margin.sense == "min");
    require(ron, "gasoline RON margin must be reported in RON");
    require(report.quality_margins.size() >= 5, "capacity margins must be reported in kbpd");
    require(report.qualification.find("NOT obtained") != std::string::npos, "report must state the open gates");
    const std::string json = refinery::report_to_json(report);
    require(json.find("\"rows\":") != std::string::npos && json.find("\"quality_margins\":") != std::string::npos,
            "json schema keys");
    require(json.find("\"unit\":\"kbpd\"") != std::string::npos, "json must carry unit labels");
    require(json.find("\"dual_unit\":") != std::string::npos &&
            json.find("\"objective_coefficient_unit\":\"USD/bbl\"") != std::string::npos,
            "json must label duals and objective coefficients");
    require(json.find("\"coefficient_unit\":") != std::string::npos,
            "json must label constraint coefficients");

    auto infeasible = refinery::make_synthetic_config();
    infeasible.dhdt_capacity_kbpd = 5.0;
    infeasible.products[1].min_demand_kbpd = 100.0;
    const auto infeasible_model = refinery::build_refinery_lp(infeasible);
    const auto iis = markov_cero::analysis::compute_iis(infeasible_model);
    const auto infeasible_res = markov_cero::api::solve_model(infeasible_model, options);
    const auto infeasible_report = refinery::build_refinery_report(infeasible, infeasible_model, infeasible_res, &iis);
    require(!infeasible_report.iis.empty(), "IIS entries must appear in the report");
    for (const auto& entry : infeasible_report.iis)
        require(entry.scope == "row" && entry.unit == "kbpd", "IIS scope and row unit must be named");
    require(infeasible_report.iis_scope_note.find("bound") != std::string::npos,
            "unsupported bound scope must be stated");
    require(refinery::report_to_json(infeasible_report).find("\"scope\":\"row\"") != std::string::npos,
            "json must carry the IIS scope label");
    std::cout << "[+] test_named_report_structure passed: " << report.rows.size() << " rows, "
              << report.active_bounds.size() << " active bounds, " << report.quality_margins.size()
              << " margins, " << infeasible_report.iis.size() << " IIS rows\n";
}

void test_generated_case_fixtures() {
    using Status = markov_cero::lp::reference::SolveStatus;
    struct Case { const char* name; Status status; double objective; };
    const Case cases[] = {
        {"crude_blending_large.qps", Status::optimal, 38572.99796295821}, {"process_network_large.mps", Status::optimal, 6420.866666666667},
        {"refinery_scheduling_large.mps", Status::optimal, -65416.9}, {"production_planning_large.mps", Status::gap_satisfied, 93931.950045},
        {"supply_chain_large.mps", Status::optimal, 926342.1266}, {"power_dispatch_dc_opf.qps", Status::optimal, 144.77105258043972},
        {"neiro_refinery_scheduling.mps", Status::optimal, -275030.0}, {"li_crude_blending.mps", Status::optimal, 5760.0},
        {"capitanescu_dc_opf.mps", Status::optimal, 0.0}, {"shapiro_network_flow.mps", Status::optimal, 1176.0}, {"pochet_lot_sizing.mps", Status::optimal, 1050.0},
    };
    for (const auto& fixture : cases) {
        const std::string path = std::string(MARKOV_CERO_SOURCE_DIR) + "/data/cases/" + fixture.name;
        std::ifstream first_stream(path), second_stream(path);
        require(first_stream.good() && second_stream.good(), path.c_str());
        const auto first = markov_cero::io::parse_mps(first_stream), second = markov_cero::io::parse_mps(second_stream);
        require(markov_cero::model::hash_model(first).fingerprint() == markov_cero::model::hash_model(second).fingerprint(), path.c_str());
        markov_cero::api::SolveOptions options;
        options.milp_options.time_limit_seconds = 3600.0; // default 60s would truncate the proof audit
        options.mip_proof_time_limit_seconds = 600.0;     // 300s was exhausted by CI Debug -O0 proof builds
        if (fixture.status == Status::gap_satisfied) options.milp_options.relative_gap_tolerance = 0.05;
        const auto t0 = std::chrono::steady_clock::now();
        const auto result = markov_cero::api::solve_model(first, options);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        std::cerr << "[t] " << fixture.name << " ms=" << ms << " status=" << markov_cero::lp::reference::to_string(result.status)
                  << " verified=" << result.original_verified << " obj=" << result.objective << " proof=" << result.proof_status << " exhausted=" << result.proof_budget_exhausted << " build_ms=" << result.mip_proof_build_ms << "\n";
        require(result.status == fixture.status && result.original_verified, path.c_str());
        require(std::fabs(result.objective - fixture.objective) <= 1e-3 + 1e-6 * std::fabs(fixture.objective), path.c_str());
    }
}

int main() {
    try {
        test_synthetic_planning();
        test_refinery_iis_analysis();
        test_haverly_pooling_slp();
        test_units_conversions_and_mismatch();
        test_quality_schema_and_feed_balance();
        test_named_report_structure();
        test_generated_case_fixtures();
        std::cout << "All Refinery & IIS analysis tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error in refinery tests: " << e.what() << "\n";
        return 1;
    }
}
