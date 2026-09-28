#include "markov_cero/nlp/lbfgs.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace markov_cero::nlp {

void Lbfgs::update(const std::vector<double>& s, const std::vector<double>& y) {
    if (s.size() != y.size() || s.empty()) {
        return;
    }
    double curvature = 0.0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        curvature += s[i] * y[i];
    }
    // Non-positive curvature would make B indefinite; skip the pair.
    if (!std::isfinite(curvature) || curvature <= 1e-12) {
        return;
    }
    // gamma estimates the initial scaling H_0 = gamma * I (Nocedal & Wright 7.20).
    double yy = 0.0;
    for (std::size_t i = 0; i < y.size(); ++i) {
        yy += y[i] * y[i];
    }
    if (yy > 0.0) {
        gamma_ = curvature / yy;
    }
    s_history_.push_back(s);
    y_history_.push_back(y);
    rho_history_.push_back(1.0 / curvature);
    if (s_history_.size() > memory_) {
        s_history_.erase(s_history_.begin());
        y_history_.erase(y_history_.begin());
        rho_history_.erase(rho_history_.begin());
    }
}

std::vector<double> Lbfgs::search_direction(const std::vector<double>& grad) const {
    const std::size_t n = grad.size();
    std::vector<double> q = grad;
    const std::size_t pairs = s_history_.size();
    std::vector<double> alpha(pairs, 0.0);

    // Loop 1 (newest to oldest): alpha_i = rho_i s_i^T q; q -= alpha_i y_i.
    for (std::size_t idx = pairs; idx-- > 0;) {
        double sq = 0.0;
        const auto& s = s_history_[idx];
        for (std::size_t i = 0; i < s.size(); ++i) {
            sq += s[i] * q[i];
        }
        alpha[idx] = rho_history_[idx] * sq;
        const auto& y = y_history_[idx];
        for (std::size_t i = 0; i < y.size(); ++i) {
            q[i] -= alpha[idx] * y[i];
        }
    }

    // H_0 scaling.
    std::vector<double> r(n);
    for (std::size_t i = 0; i < n; ++i) {
        r[i] = gamma_ * q[i];
    }

    // Loop 2 (oldest to newest): beta = rho_i y_i^T r; r += s_i (alpha_i - beta).
    for (std::size_t idx = 0; idx < pairs; ++idx) {
        double yr = 0.0;
        const auto& y = y_history_[idx];
        for (std::size_t i = 0; i < y.size(); ++i) {
            yr += y[i] * r[i];
        }
        const double beta = rho_history_[idx] * yr;
        const auto& s = s_history_[idx];
        for (std::size_t i = 0; i < s.size(); ++i) {
            r[i] += s[i] * (alpha[idx] - beta);
        }
    }

    // d = -H grad.
    for (std::size_t i = 0; i < n; ++i) {
        r[i] = -r[i];
    }
    return r;
}

std::vector<double> Lbfgs::hessian_matrix(std::size_t dimension) const {
    if (!s_history_.empty() && s_history_.back().size() != dimension) {
        throw std::invalid_argument("L-BFGS Hessian dimension does not match its history");
    }
    const double initial_scale = std::min(1.0 / std::max(gamma_, 1e-12), 1e3);
    std::vector<double> B(dimension * dimension, 0.0);
    for (std::size_t i = 0; i < dimension; ++i) B[i * dimension + i] = initial_scale;

    // Apply the direct BFGS rank-two Hessian updates from the retained pairs:
    // B+ = B - (Bs)(Bs)^T/(s^T Bs) + yy^T/(s^T y).
    for (std::size_t p = 0; p < s_history_.size(); ++p) {
        const auto& s = s_history_[p];
        const auto& y = y_history_[p];
        const double sy = 1.0 / rho_history_[p];
        std::vector<double> Bs(dimension, 0.0);
        for (std::size_t i = 0; i < dimension; ++i)
            for (std::size_t j = 0; j < dimension; ++j)
                Bs[i] += B[i * dimension + j] * s[j];
        double sBs = 0.0;
        for (std::size_t i = 0; i < dimension; ++i) sBs += s[i] * Bs[i];
        if (!(sy > 0.0) || !(sBs > 1e-18) || !std::isfinite(sBs)) continue;
        for (std::size_t i = 0; i < dimension; ++i) {
            for (std::size_t j = 0; j < dimension; ++j) {
                B[i * dimension + j] += y[i] * y[j] / sy - Bs[i] * Bs[j] / sBs;
            }
        }
    }
    return B;
}

} // namespace markov_cero::nlp
