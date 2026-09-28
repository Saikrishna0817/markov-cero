#include "markov_cero/refinery/pooling_slp.hpp"

#include "markov_cero/api/solve.hpp"

#include <chrono>
#include <cmath>

namespace markov_cero::refinery {

HaverlyResult solve_haverly_pooling(double initial_quality_guess,
                                    std::size_t max_iterations,
                                    double tolerance) {
    const auto start_time = std::chrono::steady_clock::now();
    HaverlyResult res;
    res.final_pool_quality = initial_quality_guess;

    double q_curr = initial_quality_guess;

    for (std::size_t iter = 0; iter < max_iterations; ++iter) {
        ++res.iterations;

        // Variables:
        // 0: xA (Cost 6, S 3.0)
        // 1: xB (Cost 16, S 1.0)
        // 2: y1 (Pool to P1)
        // 3: y2 (Pool to P2)
        // 4: z1 (C to P1, Cost 10, S 2.0)
        // 5: z2 (C to P2, Cost 10, S 2.0)
        // 6: P1 (Price 9, max 100, S <= 2.5)
        // 7: P2 (Price 15, max 200, S <= 1.5)
        model::Model model;
        model.name = "HAVERLY_SLP_ITER_" + std::to_string(iter);
        model.objective_sense = model::ObjectiveSense::minimize;
        model.objective_offset = 0.0;

        const std::size_t n = 8;
        model.variable_name = {"xA", "xB", "y1", "y2", "z1", "z2", "P1", "P2"};
        model.variable_lower = std::vector<model::Bound>(n, model::Bound::finite(0.0));
        model.variable_upper = std::vector<model::Bound>(n, model::Bound::positive_infinity());
        model.variable_upper[6] = model::Bound::finite(100.0); // P1 max demand
        model.variable_upper[7] = model::Bound::finite(200.0); // P2 max demand
        model.variable_type = std::vector<model::VariableType>(n, model::VariableType::continuous);

        // Objective: Minimize Cost - Revenue = 6*xA + 16*xB + 10*(z1+z2) - 9*P1 - 15*P2
        model.objective = {6.0, 16.0, 0.0, 0.0, 10.0, 10.0, -9.0, -15.0};

        model::SparseMatrixBuilder b(5, n);
        // Row 0: Pool Mass Balance: xA + xB - y1 - y2 = 0
        model.row_name.push_back("POOL_BALANCE");
        model.row_lower.push_back(model::Bound::finite(0.0));
        model.row_upper.push_back(model::Bound::finite(0.0));
        b.add(0, 0, 1.0); b.add(0, 1, 1.0); b.add(0, 2, -1.0); b.add(0, 3, -1.0);

        // Row 1: Product 1 Volume: y1 + z1 - P1 = 0
        model.row_name.push_back("P1_BALANCE");
        model.row_lower.push_back(model::Bound::finite(0.0));
        model.row_upper.push_back(model::Bound::finite(0.0));
        b.add(1, 2, 1.0); b.add(1, 4, 1.0); b.add(1, 6, -1.0);

        // Row 2: Product 2 Volume: y2 + z2 - P2 = 0
        model.row_name.push_back("P2_BALANCE");
        model.row_lower.push_back(model::Bound::finite(0.0));
        model.row_upper.push_back(model::Bound::finite(0.0));
        b.add(2, 3, 1.0); b.add(2, 5, 1.0); b.add(2, 7, -1.0);

        // Row 3: Product 1 Sulfur Spec: q_curr * y1 + 2.0 * z1 <= 2.5 * P1
        // => q_curr * y1 + 2.0 * z1 - 2.5 * P1 <= 0
        model.row_name.push_back("P1_SULFUR_SPEC");
        model.row_lower.push_back(model::Bound::negative_infinity());
        model.row_upper.push_back(model::Bound::finite(0.0));
        b.add(3, 2, q_curr); b.add(3, 4, 2.0); b.add(3, 6, -2.5);

        // Row 4: Product 2 Sulfur Spec: q_curr * y2 + 2.0 * z2 <= 1.5 * P2
        // => q_curr * y2 + 2.0 * z2 - 1.5 * P2 <= 0
        model.row_name.push_back("P2_SULFUR_SPEC");
        model.row_lower.push_back(model::Bound::negative_infinity());
        model.row_upper.push_back(model::Bound::finite(0.0));
        b.add(4, 3, q_curr); b.add(4, 5, 2.0); b.add(4, 7, -1.5);

        model.matrix = b.build();
        model.validate();

        api::SolveOptions opts;
        opts.engine = "simplex";
        opts.enable_presolve = false;
        const auto sol = api::solve_model(model, opts);

        if (sol.status != lp::reference::SolveStatus::optimal) {
            break;
        }

        const auto& x = sol.primal;
        const double xA = x[0];
        const double xB = x[1];
        const double total_pool_in = xA + xB;
        
        double q_new = q_curr;
        if (total_pool_in > 1e-6) {
            q_new = (3.0 * xA + 1.0 * xB) / total_pool_in;
        }

        const double diff = std::abs(q_new - q_curr);
        q_curr = 0.5 * q_curr + 0.5 * q_new; // Damped SLP step

        res.flow_xA = xA;
        res.flow_xB = xB;
        res.flow_y1 = x[2];
        res.flow_y2 = x[3];
        res.flow_z1 = x[4];
        res.flow_z2 = x[5];
        res.profit = -sol.objective;
        res.final_pool_quality = q_curr;

        if (diff <= tolerance) {
            res.converged = true;
            break;
        }
    }

    const auto elapsed = std::chrono::steady_clock::now() - start_time;
    res.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
    return res;
}

} // namespace markov_cero::refinery
