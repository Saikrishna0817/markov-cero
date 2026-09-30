#include "dual_simplex_internal.hpp"
namespace markov_cero::lp::dual {
using namespace detail_dual_simplex;
Result solve_impl(const transform::SparseCanonicalModel& m, const Options& o,
                  const std::optional<BasisState>& warm, FactorCache* cache) {
    Result out;
    if (cache) {
        cache->reused_last = false;
    }
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
            if (cache) {
                cache->valid = false;
            }
            return cold(m, o, "cold solve delegated to certified M3 oracle");
        }
        validate_basis_metadata(m, *warm);
        // Reuse is gated on the exact (model fingerprint, basis) pair the
        // stored factorization was built for; anything else refactors.
        const bool reusable = cache && cache->valid &&
                              cache->model_fingerprint == warm->model_fingerprint &&
                              cache->basic_variables == warm->basic_variables;
        if (cache) {
            cache->reused_last = reusable;
            cache->model_fingerprint = warm->model_fingerprint;
            cache->valid = false;
            if (!reusable) {
                cache->basic_variables = warm->basic_variables;
                cache->dse_weights.clear();
            }
        }
        linalg::SparseBasisFactorization scratch_factor;
        std::vector<std::size_t> scratch_basis;
        std::vector<double> scratch_weights;
        linalg::SparseBasisFactorization* factor_p;
        std::vector<std::size_t>* basis_p;
        std::vector<double>* weights_p;
        if (cache) {
            factor_p = &cache->factor;
            basis_p = &cache->basic_variables;
            weights_p = &cache->dse_weights;
        } else {
            scratch_basis = warm->basic_variables;
            factor_p = &scratch_factor;
            basis_p = &scratch_basis;
            weights_p = &scratch_weights;
        }
        if (!reusable) {
            try {
                *factor_p = linalg::SparseBasisFactorization::factorize(
                    sparse_basis_matrix(m, *basis_p), sparse_options(o));
            } catch (const std::exception&) {
                throw std::invalid_argument("warm basis is singular");
            }
        }
        auto& factor = *factor_p;
        auto& basis = *basis_p;
        auto& dse_weights = *weights_p;
        const std::size_t base_refactorizations =
            reusable ? factor.statistics().refactorizations : 0;
        out.used_warm_start = true;
        out.factor_reused = reusable;
        out.telemetry.reserve(std::min(o.iteration_limit, o.telemetry_limit));
        out.refactorizations = factor.statistics().refactorizations - base_refactorizations;
        // Exact steepest-edge weights are a property of the stored
        // factorization: a reuse inherits them, a fresh factor recomputes.
        if (o.pricing == PricingPolicy::steepest_edge && dse_weights.size() != m.matrix.rows) {
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
            auto aty = m.multiply_transpose(y);
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
                certified.factor_reused = out.factor_reused;
                certified.telemetry = std::move(out.telemetry);
                certified.telemetry_truncated = out.telemetry_truncated;
                certified.refactorizations = out.refactorizations;
                certified.solution.condition_estimate = factor.current_condition_estimate();
                if (cache && certified.solution.status == reference::SolveStatus::optimal) {
                    // The stored factor and weights now describe the certified
                    // optimal basis; keep them for the next resolve.
                    cache->valid = true;
                }
                return certified;
            }
            std::vector<double> e(m.matrix.rows);
            e[leaving] = 1;
            auto pi = factor.solve_transpose(e);
            auto alpha = m.multiply_transpose(pi);
            double best_ratio = 0;
            const std::size_t entering =
                select_entering_column(m, alpha, rc, is_basic, o, best_ratio);
            if (entering == m.matrix.columns) {
                auto certified = certified_farkas(m, pi, o);
                certified.used_warm_start = true;
                certified.factor_reused = out.factor_reused;
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
            // Column extraction walks only the CSC slice of the entering
            // column (O(nnz of column)), not the full model validation a
            // generic dense_column() helper would redo every pivot.
            std::vector<double> entering_column(m.matrix.rows, 0.0);
            for (std::size_t p = m.matrix.column_offsets[entering];
                 p < m.matrix.column_offsets[entering + 1]; ++p) {
                entering_column[m.matrix.row_indices[p]] = m.matrix.values[p];
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
            if (o.pricing != PricingPolicy::steepest_edge) dse_weights.clear();
            if (factor.needs_refactorization()) {
                factor.refactorize();
                if (o.pricing == PricingPolicy::steepest_edge) {
                    dse_weights = compute_exact_dse_weights(m, factor);
                }
            } else if (o.pricing == PricingPolicy::steepest_edge && (step + 1) % 500 == 0) {
                dse_weights = compute_exact_dse_weights(m, factor);
            }
            out.refactorizations = factor.statistics().refactorizations - base_refactorizations;
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

Result solve_verified(const transform::SparseCanonicalModel& m, const Options& o,
                      const std::optional<BasisState>& warm, FactorCache* cache) {
    return verify_accepted(m, o, solve_impl(m, o, warm, cache));
}

Result solve(const transform::SparseCanonicalModel& m, const Options& o,
             const std::optional<BasisState>& warm) {
    return solve_verified(m, o, warm, nullptr);
}

Result solve(const transform::CanonicalModel& m, const Options& o,
             const std::optional<BasisState>& warm) {
    return solve(transform::sparse_from_dense(m), o, warm);
}
}
