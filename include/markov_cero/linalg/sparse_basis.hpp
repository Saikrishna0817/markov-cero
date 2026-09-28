#pragma once
#include <cstddef>
#include <utility>
#include <vector>
namespace markov_cero::linalg {
struct SparseCsc {
    std::size_t rows{};
    std::size_t columns{};
    std::vector<std::size_t> column_offsets;
    std::vector<std::size_t> row_indices;
    std::vector<double> values;
    void validate(std::size_t maximum_nonzeros = 4U * 1024U * 1024U) const;
    [[nodiscard]] std::vector<double> dense_column(std::size_t column) const;
    [[nodiscard]] static SparseCsc from_columns(std::size_t rows,
                                                const std::vector<std::vector<double>>& columns);
};
struct SparseLuDiagnostics {
    std::size_t lower_nonzeros{};
    std::size_t upper_nonzeros{};
    std::size_t factor_nonzeros{};
    double minimum_absolute_pivot{};
    double maximum_absolute_pivot{};
    double growth_factor{};
};
struct SparseLuSymbolicAnalysis {
    std::size_t dimension{0};
    std::vector<std::size_t> column_order;     // position -> original column
    std::vector<std::size_t> column_position;  // original column -> position
};
class SparseLu final {
  public:
    /// Perform symbolic analysis (fill-reducing column ordering) once on the sparsity pattern.
    static SparseLuSymbolicAnalysis analyze_sparsity(const SparseCsc& matrix, bool reduce_fill = true);

    /// Fast numeric factorization reusing precomputed symbolic analysis.
    static SparseLu factorize_numeric(const SparseCsc& matrix,
                                      const SparseLuSymbolicAnalysis& symbolic,
                                      double singular_tolerance = 1e-14,
                                      std::size_t maximum_factor_nonzeros = 4U * 1024U * 1024U);

    /// R6: `reduce_fill` enables the minimum-degree column ordering before
    /// elimination (position -> original column, stored in `column_order_` and
    /// inverted in the solves). Ordering changes fill cost, never the answer.
    static SparseLu factorize(const SparseCsc& matrix, double singular_tolerance = 1e-14,
                              std::size_t maximum_factor_nonzeros = 4U * 1024U * 1024U,
                              bool reduce_fill = true);
    [[nodiscard]] std::vector<double> solve(const std::vector<double>& rhs) const;
    [[nodiscard]] std::vector<double> solve_refined(const SparseCsc& matrix,
                                                    const std::vector<double>& rhs,
                                                    std::size_t max_steps = 2,
                                                    double early_exit_tol = 1e-14) const;
    [[nodiscard]] std::vector<double> solve_transpose(const std::vector<double>& rhs) const;
    [[nodiscard]] const SparseLuDiagnostics& diagnostics() const noexcept { return diagnostics_; }
    [[nodiscard]] std::size_t dimension() const noexcept { return dimension_; }

  private:
    using Entry = std::pair<std::size_t, double>;
    std::size_t dimension_{};
    std::vector<std::vector<Entry>> lower_rows_;
    std::vector<std::vector<Entry>> upper_rows_;
    std::vector<std::vector<Entry>> lower_columns_;
    std::vector<std::vector<Entry>> upper_columns_;
    std::vector<std::size_t> row_order_;
    std::vector<std::size_t> column_order_;  // R6: position -> original column
    SparseLuDiagnostics diagnostics_;
};
struct SparseBasisOptions {
    double singular_tolerance{1e-14};
    double update_pivot_tolerance{1e-12};
    double eta_density_trigger{0.5};
    std::size_t maximum_updates{64};
    std::size_t maximum_dimension{4096};
    std::size_t maximum_nonzeros{4U * 1024U * 1024U};
    std::size_t maximum_factor_nonzeros{4U * 1024U * 1024U};
    // RW-8 (R17): selective iterative refinement. A solve pays the extra
    // O(nnz(B)) residual pass only when the product-form chain is long or the
    // base factorization shows substantial growth — the two cheap proxies for
    // accumulated drift. maximum_refinement_steps = 0 disables entirely.
    std::size_t refinement_trigger_updates{16};
    double refinement_trigger_growth{100.0};
    double refinement_trigger_condition{1e8};
    std::size_t maximum_refinement_steps{2};
    // R6: fill-reducing minimum-degree column ordering in the base LU.
    bool fill_reducing_ordering{false};
};
struct SparseBasisStatistics {
    std::size_t refactorizations{};
    std::size_t updates{};
    std::size_t current_update_chain{};
    std::size_t last_rhs_nonzeros{};
    std::size_t last_solution_nonzeros{};
    std::size_t maximum_eta_nonzeros{};
    bool update_limit_triggered{};
    bool density_triggered{};
    std::size_t refinement_attempts{};  // RW-8: solves that entered refinement
    std::size_t refinements_applied{};  // RW-8: corrections actually added
};
class SparseBasisFactorization final {
  public:
    static SparseBasisFactorization factorize(const SparseCsc& basis,
                                              const SparseBasisOptions& options = {});
    [[nodiscard]] std::vector<double> solve(const std::vector<double>& rhs);
    [[nodiscard]] std::vector<double> solve_transpose(const std::vector<double>& rhs);
    void replace_column(std::size_t position, const std::vector<double>& column);
    [[nodiscard]] bool needs_refactorization() const noexcept;
    void refactorize();
    [[nodiscard]] const SparseBasisStatistics& statistics() const noexcept { return statistics_; }
    [[nodiscard]] bool refinement_required() const noexcept;  // RW-8
    [[nodiscard]] const SparseLuDiagnostics& diagnostics() const noexcept {
        return base_.diagnostics();
    }
    /// Pivot-ratio condition proxy (max|Uii|/min|Uii|) of the CURRENT basis.
    /// Refactorizes pending eta updates first — one LU, cheap relative to a
    /// solve — so the value reflects the basis that produced the answer rather
    /// than a stale base LU. Falls back to the cached base diagnostics when the
    /// current basis will not refactorize. Empty systems report 1.0. Never
    /// throws; 0.0 is never returned for a non-empty basis (inf for a
    /// numerically singular one, matching sparse_condition_estimate).
    [[nodiscard]] double current_condition_estimate();
    [[nodiscard]] const SparseCsc& current_basis() const noexcept { return current_basis_; }

  private:
    struct Eta {
        std::size_t pivot{};
        double pivot_value{};
        std::vector<std::pair<std::size_t, double>> entries;
    };
    // RW-8: one-step iterative refinement against the product-form operator.
    [[nodiscard]] std::vector<double> refine(std::vector<double> x,
                                             const std::vector<double>& rhs, bool transpose);
    void apply_updates(std::vector<double>& x) const;
    void apply_updates_transpose(std::vector<double>& x) const;
    [[nodiscard]] std::vector<double> residual_vector(const std::vector<double>& rhs,
                                                      const std::vector<double>& x,
                                                      bool transpose) const;
    SparseBasisOptions options_;
    SparseCsc current_basis_;
    SparseLu base_;
    std::vector<Eta> updates_;
    SparseBasisStatistics statistics_;
};
[[nodiscard]] double sparse_infinity_residual(const SparseCsc& matrix, const std::vector<double>& x,
                                              const std::vector<double>& rhs,
                                              bool transpose = false);
// RW-8: cheap pivot-ratio condition proxy from the base LU (stale after eta
// updates — a screening signal for the robustness dossier, not a true kappa).
[[nodiscard]] double sparse_condition_estimate(const SparseLuDiagnostics& diagnostics);
} // namespace markov_cero::linalg
