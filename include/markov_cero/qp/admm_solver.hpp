#pragma once

#include "markov_cero/qp/kkt.hpp"
#include "markov_cero/qp/model.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <optional>
#include <vector>

namespace markov_cero::qp {

enum class QpStatus {
    optimal,
    primal_infeasible,
    dual_infeasible,
    iteration_limit,
    time_limit,
    non_convex,
    unsupported,
    numerical_error
};

[[nodiscard]] const char* to_string(QpStatus status) noexcept;

struct QpOptions {
    double absolute_tolerance{1e-4};
    double relative_tolerance{1e-4};
    double primal_infeasible_tolerance{1e-5};
    double dual_infeasible_tolerance{1e-5};
    double sigma{1e-6};
    double rho_init{0.1};
    double alpha{1.6};
    std::size_t max_iterations{4000};
    double time_limit_seconds{60.0};
    std::optional<std::chrono::steady_clock::time_point> deadline;
    bool adaptive_rho{true};
    std::size_t adaptive_rho_interval{25};
    bool verbose{false};
    // Repeated-solve optimization: let the solver's KktSolver keep the symbolic
    // pattern cache across solves of an unchanged KKT shape (see kkt.hpp).
    // false restores a full symbolic factorization on every solve and is the
    // A/B control for "cache miss == current behavior".
    bool reuse_kkt_symbolic{true};
    // IR-21 (contract resource-limits.md §4): solve-wide allocation budget
    // hooks for the KKT factor workspace — the memory counterpart of
    // `deadline`, plain function pointers so this layer stays free of core
    // includes. `charge_bytes(user, n)` returns false when the budget refuses
    // n bytes (the owner records the stop); `release_bytes(user, n)` hands
    // them back. Set both together or leave both null.
    bool (*charge_bytes)(void* user, std::size_t bytes) noexcept = nullptr;
    void (*release_bytes)(void* user, std::size_t bytes) noexcept = nullptr;
    void* charge_user = nullptr;
};

struct QpSolution {
    QpStatus status{QpStatus::numerical_error};
    std::string message;
    double objective_value{0.0};
    std::vector<double> x;
    std::vector<double> z;
    std::vector<double> y;
    std::vector<double> infeasibility_certificate;
    std::vector<double> unbounded_ray;
    std::size_t iterations{0};
    double solve_time_seconds{0.0};
    double primal_residual{0.0};
    double dual_residual{0.0};
    std::size_t refactorization_count{0};
    // Pivot-ratio condition proxy of the KKT LDL^T diagonal (max|D|/min|D|).
    // 0.0 when the KKT system was never factorized.
    double condition_estimate{0.0};
    // Telemetry for D-08: true when the GPU ADMM residual path was actually
    // active (backend requested + thresholds met + CUDA device present).
    bool gpu_path_active{false};
    // Repeated-solve telemetry: how many KKT symbolic factorizations this solve
    // skipped because the KKT pattern still matched the cached one.
    std::size_t kkt_symbolic_reuse{0};
    // Set only after the accepted witness passes the independent QP verifier.
    bool verified{false};
};

class AdmmQpSolver {
public:
    explicit AdmmQpSolver(QpOptions options = {}) : options_(options) {}

    /// Repeated-solve session object: one instance reused across solves keeps
    /// the KKT symbolic pattern cache (and its counters) alive, so a repeat of
    /// an unchanged KKT shape skips symbolic factorization. Call solve()
    /// sequentially on a single instance; use a fresh instance for parallel
    /// solves.
    [[nodiscard]] QpSolution solve(const QuadraticModel& model);

    // W3/D-08: request the GPU-assisted residual path. Activation still
    // requires the locked thresholds (NNZ(P) > 100,000 plus a CUDA build with
    // an sm_50+ device); an unmet request falls back to CPU silently.
    void set_gpu_backend(bool enable) { gpu_requested_ = enable; }

    // Per-solve options (deadline, limits, tolerance, cache toggle).
    void set_options(const QpOptions& options) { options_ = options; }
    [[nodiscard]] const QpOptions& options() const noexcept { return options_; }

    // Repeated-solve telemetry of the KKT factor cache for this solver.
    [[nodiscard]] std::size_t symbolic_factorizations() const noexcept {
        return kkt_.symbolic_factorizations();
    }
    [[nodiscard]] std::size_t symbolic_reuses() const noexcept { return kkt_.symbolic_reuses(); }

private:
    QpOptions options_;
    bool gpu_requested_{false};
    KktSolver kkt_;
};

/// Reported when the sparse KKT factor charge exceeds the solve-wide memory
/// budget (contract resource-limits.md §4): a public output string naming
/// which workspace refused.
inline constexpr const char* kKktBudgetMessage =
    "sparse QP KKT factor exceeds the solve memory budget";

/// High-level function to solve a QuadraticModel using ADMM.
[[nodiscard]] QpSolution solve_qp(const QuadraticModel& model, const QpOptions& options = {});

/// solve_qp variant honoring the W3/D-08 GPU request. The request activates
/// the GPU residual path only when NNZ(P) > 100,000 and a CUDA device is
/// available; otherwise it silently matches solve_qp.
[[nodiscard]] QpSolution solve_qp(const QuadraticModel& model, const QpOptions& options,
                                  bool gpu_backend_requested);

/// Repeated-solve variant: equilibration and option deadlines still run per
/// call, but the KKT factor cache lives in the caller-owned `session`, so
/// repeated solves with an unchanged KKT pattern skip symbolic work.
[[nodiscard]] QpSolution solve_qp(const QuadraticModel& model, const QpOptions& options,
                                  AdmmQpSolver& session, bool gpu_backend_requested);

} // namespace markov_cero::qp
