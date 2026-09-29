#pragma once

#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"

#include <cstddef>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace markov_cero::lp::dual {

// Pricing policies:
// - bland: smallest index anti-cycling rule
// - tableau_norm: ||A^T B^{-T} e_i||^2
// - steepest_edge: exact Forrest-Goldfarb dual steepest-edge ||B^{-T} e_i||^2 with O(m) updates
enum class PricingPolicy { bland, tableau_norm, steepest_edge };

struct BasisState {
    std::size_t rows{};
    std::size_t columns{};
    std::string model_fingerprint;
    std::vector<std::size_t> basic_variables;
};

struct Options {
    std::size_t iteration_limit{100000};
    std::size_t telemetry_limit{10000};
    double feasibility_tolerance{1e-9};
    double dual_tolerance{1e-9};
    double pivot_tolerance{1e-12};
    double condition_trigger{1e-14};
    bool harris_ratio{true};
    bool allow_cold_fallback{true};
    PricingPolicy pricing{PricingPolicy::steepest_edge};
    std::optional<std::chrono::steady_clock::time_point> deadline;
};

struct IterationRecord {
    std::size_t iteration{};
    double objective{};
    double most_negative_basic{};
    std::size_t leaving{};
    std::size_t entering{};
    double pivot{};
    bool harris{};
};

struct Result {
    reference::Result solution;
    BasisState basis_state;
    bool used_warm_start{};
    bool used_cold_fallback{};
    // True when this solve reused the cached basis factorization (and, for
    // steepest-edge pricing, the cached exact weights) instead of rebuilding
    // them from the warm basis.
    bool factor_reused{};
    // True when an accepted result (optimal / infeasible / unbounded) was
    // re-checked by verify::verify_reference_result and passed. Every accepting
    // path runs that check; reuse must never skip it.
    bool verified{};
    bool telemetry_truncated{};
    std::size_t refactorizations{};
    std::size_t perturbation_cleanups{};
    std::vector<IterationRecord> telemetry;
    std::string message;
};

// Factorization state carried across repeated RHS-only / bound-only resolves.
// `model_fingerprint` and `basic_variables` describe exactly what the stored
// factor represents: solve() only reuses the factor when both still match the
// warm basis handed to it, so a stale entry can never be applied.
struct FactorCache {
    linalg::SparseBasisFactorization factor;
    std::string model_fingerprint;
    std::vector<std::size_t> basic_variables;
    std::vector<double> dse_weights;
    bool valid{false};
    bool reused_last{false};
};

// Repeated-solve session: keeps the accepted basis and its factorization
// between resolves so RHS-only / bound-only changes skip the basis refactor.
// Every accepted result is verified; a degenerate, infeasible, or rejected
// result drops the cached basis and the next resolve falls back to a cold solve.
class Session {
  public:
    Session() = default;
    explicit Session(const Options& defaults) : defaults_(defaults) {}

    Result resolve(const transform::CanonicalModel& model, const Options& options);
    Result resolve(const transform::CanonicalModel& model) { return resolve(model, defaults_); }

    [[nodiscard]] const Options& defaults() const noexcept { return defaults_; }
    [[nodiscard]] std::size_t resolve_count() const noexcept { return resolve_count_; }
    [[nodiscard]] std::size_t factor_reuse_count() const noexcept { return factor_reuse_count_; }
    [[nodiscard]] std::size_t cold_fallback_count() const noexcept { return cold_fallback_count_; }
    [[nodiscard]] std::size_t verified_count() const noexcept { return verified_count_; }
    [[nodiscard]] bool last_verified() const noexcept { return last_verified_; }
    [[nodiscard]] const std::optional<BasisState>& basis() const noexcept { return basis_; }
    [[nodiscard]] const FactorCache& cache() const noexcept { return cache_; }

    // Drops the basis and the cached factorization; counters are retained.
    void reset() noexcept {
        basis_.reset();
        cache_ = FactorCache{};
        last_verified_ = false;
    }

  private:
    Options defaults_{};
    std::optional<BasisState> basis_;
    FactorCache cache_;
    std::size_t resolve_count_{0};
    std::size_t factor_reuse_count_{0};
    std::size_t cold_fallback_count_{0};
    std::size_t verified_count_{0};
    bool last_verified_{false};
};

[[nodiscard]] std::string fingerprint(const transform::CanonicalModel& model);
[[nodiscard]] BasisState make_basis_state(const transform::CanonicalModel& model,
                                          const std::vector<std::size_t>& basic_variables);
[[nodiscard]] std::string serialize_basis(const BasisState& basis);
[[nodiscard]] BasisState parse_basis(const std::string& text);
[[nodiscard]] Result solve(const transform::CanonicalModel& model, const Options& options = {},
                           const std::optional<BasisState>& warm_start = std::nullopt);
[[nodiscard]] Session make_session(const Options& defaults = {});

} // namespace markov_cero::lp::dual
