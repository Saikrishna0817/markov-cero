#include "dual_simplex_internal.hpp"
namespace markov_cero::lp::dual {
using namespace detail_dual_simplex;
Result solve(const transform::CanonicalModel& m, const Options& o,
             const std::optional<BasisState>& warm) {
    Result out;
    try {
        m.validate();
    } catch (const std::exception& e) {
        out.solution.status = reference::SolveStatus::invalid_model;
        out.solution.message = e.what();
        out.message = e.what();
        return out;
    }
    try {
        validate_options(o);
    } catch (const std::exception& e) {
        out.solution.status = reference::SolveStatus::invalid_options;
        out.solution.message = e.what();
        out.message = e.what();
        return out;
    }
    try {
        if (m.matrix.rows > maximum_rows || m.matrix.columns > maximum_columns) {
            throw std::length_error("dual simplex reference dimension limit exceeded");
        }
        check_product(m.matrix.rows, m.matrix.rows);
        if (!warm) {
            return cold(m, o, "cold solve delegated to certified M3 oracle");
        }
        validate_basis_metadata(m, *warm);
        auto basis = warm->basic_variables;
        linalg::SparseBasisFactorization factor;
        try {
            factor = linalg::SparseBasisFactorization::factorize(sparse_basis_matrix(m, basis),
                                                                 sparse_options(o));
        } catch (const std::exception&) {
            throw std::invalid_argument("warm basis is singular");
        }
        out.used_warm_start = true;
        out.telemetry.reserve(std::min(o.iteration_limit, o.telemetry_limit));
        out.refactorizations = factor.statistics().refactorizations;
        std::vector<double> dse_weights;
        if (o.pricing == PricingPolicy::steepest_edge) {
            dse_weights = compute_exact_dse_weights(m, factor);
        }
        for (std::size_t step = 0; step < o.iteration_limit; ++step) {
            if (o.deadline && std::chrono::steady_clock::now() >= *o.deadline) {
                out.solution.status = reference::SolveStatus::resource_limit;
                out.solution.message = "dual simplex wall-clock deadline reached";
                out.message = out.solution.message;
                out.solution.condition_estimate = factor.current_condition_estimate();
                return out;
            }
            const auto& diagnostics = factor.diagnostics();
            if (m.matrix.rows > 0 && diagnostics.maximum_absolute_pivot > 0 &&
                diagnostics.minimum_absolute_pivot / diagnostics.maximum_absolute_pivot <
                    o.condition_trigger) {
                throw std::runtime_error("basis condition trigger reached");
            }
            auto xb = factor.solve(m.rhs);
            std::vector<double> cb(m.matrix.rows);
            std::vector<bool> is_basic(m.matrix.columns);
            for (std::size_t i = 0; i < m.matrix.rows; ++i) {
                cb[i] = m.objective[basis[i]];
                is_basic[basis[i]] = true;
            }
            auto y = factor.solve_transpose(cb);
            auto aty = linalg::multiply_transpose(m.matrix, y);
            std::vector<double> rc(m.matrix.columns);
            for (std::size_t j = 0; j < m.matrix.columns; ++j) {
                rc[j] = m.objective[j] - aty[j];
                if (significant_negative_reduced_cost(m, j, y, rc[j], o.dual_tolerance)) {
                    if (step == 0) {
                        if (o.allow_cold_fallback) {
                            return cold(m, o, "warm basis is not dual feasible; cold fallback");
                        }
                        throw std::runtime_error("warm basis is not dual feasible");
                    }
                    throw std::runtime_error("dual feasibility lost after pivot");
                }
            }
            double worst = 0;
            const std::size_t leaving = select_leaving_row(m, factor, xb, basis, dse_weights, o, worst);
            if (leaving == m.matrix.rows) {
                auto certified = certified_optimal(m, basis, xb, y, o);
                certified.used_warm_start = true;
                certified.telemetry = std::move(out.telemetry);
                certified.telemetry_truncated = out.telemetry_truncated;
                certified.refactorizations = out.refactorizations;
                certified.solution.condition_estimate = factor.current_condition_estimate();
                return certified;
            }
            std::vector<double> e(m.matrix.rows);
            e[leaving] = 1;
            auto pi = factor.solve_transpose(e);
            auto alpha = linalg::multiply_transpose(m.matrix, pi);
            double best_ratio = 0;
            const std::size_t entering =
                select_entering_column(m, alpha, rc, is_basic, o, best_ratio);
            if (entering == m.matrix.columns) {
                auto certified = certified_farkas(m, pi, o);
                certified.used_warm_start = true;
                certified.telemetry = std::move(out.telemetry);
                certified.telemetry_truncated = out.telemetry_truncated;
                certified.refactorizations = out.refactorizations;
                certified.solution.condition_estimate = factor.current_condition_estimate();
                return certified;
            }
            if (out.telemetry.size() < o.telemetry_limit) {
                out.telemetry.push_back({step, dot(cb, xb) + m.objective_offset, xb[leaving],
                                         basis[leaving], entering, alpha[entering],
                                         o.harris_ratio});
            } else {
                out.telemetry_truncated = true;
            }
            std::vector<double> entering_column(m.matrix.rows);
            for (std::size_t i = 0; i < m.matrix.rows; ++i) {
                entering_column[i] = m.matrix(i, entering);
            }
            if (o.pricing == PricingPolicy::steepest_edge) {
                // Forrest-Goldfarb update recurrence: O(m) update
                auto aq_bar = factor.solve(entering_column);
                const double piv = aq_bar[leaving];
                if (std::abs(piv) > 1e-14 && leaving < dse_weights.size()) {
                    auto w = factor.solve(pi);
                    const double gamma_p = dse_weights[leaving];
                    for (std::size_t i = 0; i < m.matrix.rows; ++i) {
                        if (i == leaving) {
                            dse_weights[i] = std::max(1e-12, gamma_p / (piv * piv));
                        } else {
                            const double ratio = aq_bar[i] / piv;
                            const double upd =
                                dse_weights[i] - 2.0 * ratio * w[i] + ratio * ratio * gamma_p;
                            dse_weights[i] = std::max(1e-12, upd);
                        }
                    }
                }
            }
            factor.replace_column(leaving, entering_column);
            basis[leaving] = entering;
            if (factor.needs_refactorization()) {
                factor.refactorize();
                if (o.pricing == PricingPolicy::steepest_edge) {
                    dse_weights = compute_exact_dse_weights(m, factor);
                }
            } else if (o.pricing == PricingPolicy::steepest_edge && (step + 1) % 500 == 0) {
                dse_weights = compute_exact_dse_weights(m, factor);
            }
            out.refactorizations = factor.statistics().refactorizations;
        }
        out.solution.status = reference::SolveStatus::iteration_limit;
        out.solution.message = "dual simplex iteration limit";
        out.solution.condition_estimate = factor.current_condition_estimate();
        out.message = out.solution.message;
        return out;
    } catch (const std::length_error& e) {
        out.solution.status = reference::SolveStatus::resource_limit;
        out.solution.message = out.message = e.what();
        return out;
    } catch (const std::invalid_argument& e) {
        if (warm && o.allow_cold_fallback) {
            return cold(m, o, std::string("invalid warm start; cold fallback: ") + e.what());
        }
        out.solution.status = reference::SolveStatus::numerical_failure;
        out.solution.message = out.message = e.what();
        return out;
    } catch (const std::exception& e) {
        if (warm && o.allow_cold_fallback) {
            return cold(m, o, std::string("warm start numerical failure; cold fallback: ") +
                                 e.what());
        }
        out.solution.status = reference::SolveStatus::numerical_failure;
        out.solution.message = out.message = e.what();
        return out;
    }
}
}
