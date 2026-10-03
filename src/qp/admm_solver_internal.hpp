#pragma once
#include "convergence.hpp"
#include "markov_cero/qp/admm_solver.hpp"

#include "markov_cero/gpu/admm_step.hpp"
#include "markov_cero/gpu/device.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace markov_cero::qp {
namespace detail_admm_solver { void verify_accepted_result(const QuadraticModel&, QpSolution&); }
namespace detail_admm_solver {}
namespace detail_admm_solver {
constexpr std::size_t kGpuQpNnzThreshold = 100000;
}
const char* to_string(QpStatus status) noexcept;
namespace detail_admm_solver { double inf_norm(const std::vector<double>& v) noexcept; }
namespace detail_admm_solver { std::vector<double> multiply_A(const linalg::SparseCsc& A,
                               const std::vector<double>& x); }
namespace detail_admm_solver { std::vector<double> multiply_AT(const linalg::SparseCsc& A,
                                const std::vector<double>& y); }
namespace detail_admm_solver { double project_bound(double v, double l, double u) noexcept; }
namespace detail_admm_solver { bool infeasibility_certificate(const QuadraticModel& model, const QpOptions& options,
    const std::vector<double>& x, const std::vector<double>& y,
    const std::vector<double>& x_prev, const std::vector<double>& y_prev, QpSolution& sol); }

/// IR-21: live-outstanding KKT factor charge into the solve-wide allocation
/// budget (contract resource-limits.md §4). Admits growth only, hands the
/// bytes back on scope exit, and treats a refusal as "budget stop already
/// recorded by the charge hook" so the caller only has to stop the solve.
struct KktCharge {
    bool (*charge)(void*, std::size_t) noexcept;
    void (*release)(void*, std::size_t) noexcept;
    void* user;
    std::size_t bytes{0};
    explicit KktCharge(const QpOptions& o)
        : charge(o.charge_bytes), release(o.release_bytes), user(o.charge_user) {}
    KktCharge(const KktCharge&) = delete;
    KktCharge& operator=(const KktCharge&) = delete;
    ~KktCharge() {
        if (bytes != 0 && release) release(user, bytes);
    }
    [[nodiscard]] bool admit(std::size_t want) noexcept {
        if (charge == nullptr || want <= bytes) return true;
        if (!charge(user, want - bytes)) return false;
        bytes = want;
        return true;
    }
};
}
