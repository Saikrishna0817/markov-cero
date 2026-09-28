#include "markov_cero/analysis/iis_analyzer.hpp"
#include "markov_cero/api/solve.hpp"
#include "markov_cero/refinery/pooling_slp.hpp"
#include "markov_cero/refinery/refinery_model.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

void test_standard_mrpl_planning() {
    const auto cfg = markov_cero::refinery::make_standard_mrpl_config();
    const auto model = markov_cero::refinery::build_refinery_lp(cfg);

    markov_cero::api::SolveOptions options;
    options.engine = "simplex";
    options.enable_presolve = true;
    options.enable_scale = true;

    const auto res = markov_cero::api::solve_model(model, options);
    assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(res.objective < 0.0); // Profit is negative of minimized cost-revenue
    std::cout << "[+] test_standard_mrpl_planning passed: Daily Net Margin = $" 
              << -res.objective << "k / day\n";
}

void test_refinery_iis_analysis() {
    auto cfg = markov_cero::refinery::make_standard_mrpl_config();
    // Artificially create an impossible constraint conflict:
    // Demand 100 kbpd of Euro-VI Diesel, but limit DHDT capacity to 10 kbpd (while LGO sulfur cannot meet spec without DHDT)
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
    assert(res.profit > 0.0);
    assert(res.flow_xA > 0.0 || res.flow_xB > 0.0);
    std::cout << "[+] test_haverly_pooling_slp passed: converged in " << res.iterations
              << " SLP iterations, profit = $" << res.profit 
              << ", final pool quality = " << res.final_pool_quality << "%\n";
}

int main() {
    try {
        test_standard_mrpl_planning();
        test_refinery_iis_analysis();
        test_haverly_pooling_slp();
        std::cout << "All Refinery & IIS analysis tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error in refinery tests: " << e.what() << "\n";
        return 1;
    }
}
