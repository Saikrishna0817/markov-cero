#include "sparse_basis_internal.hpp"
namespace markov_cero::linalg {
using namespace detail_sparse_basis;
SparseBasisFactorization SparseBasisFactorization::factorize(const SparseCsc& basis,
                                                             const SparseBasisOptions& options) {
    validate_options(options);
    basis.validate(options.maximum_nonzeros);
    if (basis.rows != basis.columns || basis.rows > options.maximum_dimension)
        throw std::invalid_argument("invalid sparse basis dimensions");
    SparseBasisFactorization out;
    out.options_ = options;
    out.current_basis_ = basis;
    out.base_ = SparseLu::factorize(basis, options.singular_tolerance,
                                    options.maximum_factor_nonzeros,
                                    options.fill_reducing_ordering, options.deadline);
    out.statistics_.refactorizations = 1;
    return out;
}
void SparseBasisFactorization::apply_updates(std::vector<double>& x) const {
    for (const auto& eta : updates_) {
        const double xp = x[eta.pivot] / eta.pivot_value;
        require_finite(xp, "non-finite eta solve pivot");
        for (const auto& [i, value] : eta.entries)
            if (i != eta.pivot)
                x[i] -= value * xp;
        x[eta.pivot] = xp;
    }
}
void SparseBasisFactorization::apply_updates_transpose(std::vector<double>& x) const {
    for (auto it = updates_.rbegin(); it != updates_.rend(); ++it) {
        long double value = x[it->pivot];
        for (const auto& [i, a] : it->entries)
            if (i != it->pivot)
                value -= static_cast<long double>(a) * x[i];
        x[it->pivot] = static_cast<double>(value / it->pivot_value);
        require_finite(x[it->pivot], "non-finite eta transpose solve");
    }
}
std::vector<double> SparseBasisFactorization::residual_vector(const std::vector<double>& rhs,
                                                              const std::vector<double>& x,
                                                              bool transpose) const {
    // Accumulate in long double: residual must be more accurate than the solve
    // itself or refinement feeds on noise (Skeel 1980).
    const std::size_t n = current_basis_.rows;
    std::vector<long double> product(n, 0.0L);
    if (!transpose)
        for (std::size_t j = 0; j < current_basis_.columns; ++j)
            for (std::size_t p = current_basis_.column_offsets[j];
                 p < current_basis_.column_offsets[j + 1]; ++p)
                product[current_basis_.row_indices[p]] +=
                    static_cast<long double>(current_basis_.values[p]) * x[j];
    else
        for (std::size_t j = 0; j < current_basis_.columns; ++j)
            for (std::size_t p = current_basis_.column_offsets[j];
                 p < current_basis_.column_offsets[j + 1]; ++p)
                product[j] += static_cast<long double>(current_basis_.values[p]) *
                              x[current_basis_.row_indices[p]];
    std::vector<double> r(n);
    for (std::size_t i = 0; i < n; ++i)
        r[i] = static_cast<double>(static_cast<long double>(rhs[i]) - product[i]);
    return r;
}
bool SparseBasisFactorization::refinement_required() const noexcept {
    return options_.maximum_refinement_steps > 0;
}
std::vector<double> SparseBasisFactorization::refine(std::vector<double> x,
                                                     const std::vector<double>& rhs,
                                                     bool transpose) {
    for (std::size_t step = 0; step < options_.maximum_refinement_steps; ++step) {
        const std::vector<double> r = residual_vector(rhs, x, transpose);
        double worst = 0.0;
        for (double v : r)
            worst = std::max(worst, std::abs(v));
        if (worst < 1e-14)
            break;  // early exit when residual is already below noise threshold
        ++statistics_.refinement_attempts;
        std::vector<double> correction;
        if (transpose) {
            correction = r;
            apply_updates_transpose(correction);
            correction = base_.solve_transpose(correction);
        } else {
            correction = base_.solve(r);
            apply_updates(correction);
        }
        bool changed = false;
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double updated = x[i] + correction[i];
            if (updated != x[i] && std::isfinite(updated)) {
                x[i] = updated;
                changed = true;
            }
        }
        if (!changed)
            break;
        ++statistics_.refinements_applied;
    }
    return x;
}
std::vector<double> SparseBasisFactorization::solve(const std::vector<double>& rhs) {
    statistics_.last_rhs_nonzeros = count_nonzero(rhs);
    auto x = base_.solve(rhs);
    apply_updates(x);
    for (double v : x)
        require_finite(v, "non-finite eta solve result");
    if (options_.maximum_refinement_steps > 0)
        x = refine(std::move(x), rhs, false);
    statistics_.last_solution_nonzeros = count_nonzero(x);
    return x;
}
std::vector<double> SparseBasisFactorization::solve_transpose(const std::vector<double>& rhs) {
    statistics_.last_rhs_nonzeros = count_nonzero(rhs);
    if (rhs.size() != current_basis_.rows)
        throw std::invalid_argument("eta transpose dimension mismatch");
    std::vector<double> work = rhs;
    apply_updates_transpose(work);
    auto x = base_.solve_transpose(work);
    if (options_.maximum_refinement_steps > 0)
        x = refine(std::move(x), rhs, true);
    statistics_.last_solution_nonzeros = count_nonzero(x);
    return x;
}
void SparseBasisFactorization::replace_column(std::size_t position,
                                              const std::vector<double>& column) {
    if (position >= current_basis_.columns || column.size() != current_basis_.rows)
        throw std::invalid_argument("sparse basis update dimension mismatch");
    for (double v : column)
        require_finite(v, "non-finite sparse update column");
    if (needs_refactorization())
        refactorize();
    SparseCsc next;
    next.rows = current_basis_.rows;
    next.columns = current_basis_.columns;
    next.column_offsets.push_back(0);
    for (std::size_t j = 0; j < current_basis_.columns; ++j) {
        if (j == position) {
            for (std::size_t i = 0; i < column.size(); ++i)
                if (column[i] != 0) {
                    next.row_indices.push_back(i);
                    next.values.push_back(column[i]);
                }
        } else
            for (std::size_t q = current_basis_.column_offsets[j];
                 q < current_basis_.column_offsets[j + 1]; ++q) {
                next.row_indices.push_back(current_basis_.row_indices[q]);
                next.values.push_back(current_basis_.values[q]);
            }
        next.column_offsets.push_back(next.values.size());
    }
    next.validate(options_.maximum_nonzeros);
    auto direction = solve(column);
    if (std::abs(direction[position]) <= options_.update_pivot_tolerance)
        throw std::runtime_error("unstable sparse basis update pivot");
    Eta eta;
    eta.pivot = position;
    eta.pivot_value = direction[position];
    for (std::size_t i = 0; i < direction.size(); ++i)
        if (direction[i] != 0)
            eta.entries.push_back({i, direction[i]});
    auto nnz = eta.entries.size();
    updates_.push_back(std::move(eta));
    current_basis_ = std::move(next);
    ++statistics_.updates;
    statistics_.current_update_chain = updates_.size();
    statistics_.maximum_eta_nonzeros = std::max(statistics_.maximum_eta_nonzeros, nnz);
    statistics_.update_limit_triggered = updates_.size() >= options_.maximum_updates;
    statistics_.density_triggered =
        !direction.empty() &&
        static_cast<double>(nnz) / direction.size() > options_.eta_density_trigger;
}
bool SparseBasisFactorization::needs_refactorization() const noexcept {
    return statistics_.update_limit_triggered || statistics_.density_triggered;
}
void SparseBasisFactorization::refactorize() {
    // Must use the SAME ordering policy as factorize(): a hard-coded reduce_fill
    // here made refactorize() and factorize() disagree on the same matrix (the
    // ordering decides whether a pivot falls under singular_tolerance), which
    // let a basis refactorize successfully at pivot time and then fail
    // make_factor() one step later.
    base_ = SparseLu::factorize(current_basis_, options_.singular_tolerance,
                                options_.maximum_factor_nonzeros,
                                options_.fill_reducing_ordering, options_.deadline);
    updates_.clear();
    ++statistics_.refactorizations;
    statistics_.current_update_chain = 0;
    statistics_.update_limit_triggered = false;
    statistics_.density_triggered = false;
}
double SparseBasisFactorization::current_condition_estimate() {
    if (current_basis_.rows == 0 || current_basis_.columns == 0) {
        // Empty system: conventionally perfectly conditioned; a 0-dim LU has
        // no pivots and would otherwise report a degenerate inf/0.
        return 1.0;
    }
    if (!updates_.empty()) {
        // Pending eta updates make base_ an LU of an OLDER basis; recompute so
        // the proxy describes the basis that produced the current answer.
        try {
            refactorize();
        } catch (const std::exception&) {
            // The current basis will not refactorize under the solve
            // tolerances: keep the cached base-LU diagnostics (the last
            // factorization that WAS valid) rather than reporting nothing.
        }
    }
    return sparse_condition_estimate(diagnostics());
}
}
