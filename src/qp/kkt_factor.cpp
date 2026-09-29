#include "kkt_internal.hpp"
namespace markov_cero::qp {
using namespace detail_kkt;
namespace detail_kkt {
bool ldl_symbolic(std::size_t n,
                  const std::vector<std::size_t>& Ap,
                  const std::vector<std::size_t>& Ai,
                  std::vector<std::size_t>& Lp,
                  std::vector<std::size_t>& Parent,
                  std::vector<std::size_t>& Lnz,
                  std::vector<std::size_t>& Flag,
                  const std::vector<std::size_t>* P,
                  std::vector<std::size_t>* Pinv,
                  std::optional<std::chrono::steady_clock::time_point> deadline) {
    std::size_t work = 0;
    if (P && Pinv) {
        for (std::size_t k = 0; k < n; ++k) {
            (*Pinv)[(*P)[k]] = k;
        }
    }
    for (std::size_t k = 0; k < n; ++k) {
        if (deadline && std::chrono::steady_clock::now() >= *deadline) return false;
        Parent[k] = std::numeric_limits<std::size_t>::max();
        Flag[k] = k;
        Lnz[k] = 0;
        const std::size_t kk = P ? (*P)[k] : k;
        const std::size_t p2 = Ap[kk + 1];
        for (std::size_t p = Ap[kk]; p < p2; ++p) {
            if ((++work & 1023U) == 0U && deadline &&
                std::chrono::steady_clock::now() >= *deadline) return false;
            std::size_t i = Pinv ? (*Pinv)[Ai[p]] : Ai[p];
            if (i < k) {
                for (; Flag[i] != k; i = Parent[i]) {
                    if ((++work & 1023U) == 0U && deadline &&
                        std::chrono::steady_clock::now() >= *deadline) return false;
                    if (Parent[i] == std::numeric_limits<std::size_t>::max()) {
                        Parent[i] = k;
                    }
                    ++Lnz[i];
                    Flag[i] = k;
                }
            }
        }
    }
    Lp[0] = 0;
    for (std::size_t k = 0; k < n; ++k) {
        Lp[k + 1] = Lp[k] + Lnz[k];
    }
    return true;
}
}

namespace detail_kkt {
bool ldl_numeric(std::size_t n,
                 const std::vector<std::size_t>& Ap,
                 const std::vector<std::size_t>& Ai,
                 const std::vector<double>& Ax,
                 const std::vector<std::size_t>& Lp,
                 const std::vector<std::size_t>& Parent,
                 std::vector<std::size_t>& Lnz,
                 std::vector<std::size_t>& Li,
                 std::vector<double>& Lx,
                 std::vector<double>& D,
                 std::vector<double>& Y,
                 std::vector<std::size_t>& Pattern,
                 std::vector<std::size_t>& Flag,
                 const std::vector<std::size_t>* P,
                 const std::vector<std::size_t>* Pinv,
                 std::optional<std::chrono::steady_clock::time_point> deadline,
                 bool& deadline_reached) {
    std::size_t work = 0;
    for (std::size_t k = 0; k < n; ++k) {
        if (deadline && std::chrono::steady_clock::now() >= *deadline) {
            deadline_reached = true;
            return false;
        }
        Y[k] = 0.0;
        std::size_t top = n;
        Flag[k] = k;
        Lnz[k] = 0;
        const std::size_t kk = P ? (*P)[k] : k;
        const std::size_t p2 = Ap[kk + 1];
        for (std::size_t p = Ap[kk]; p < p2; ++p) {
            if ((++work & 1023U) == 0U && deadline &&
                std::chrono::steady_clock::now() >= *deadline) {
                deadline_reached = true;
                return false;
            }
            std::size_t i = Pinv ? (*Pinv)[Ai[p]] : Ai[p];
            if (i <= k) {
                Y[i] += Ax[p];
                std::size_t len = 0;
                for (; Flag[i] != k; i = Parent[i]) {
                    Pattern[len++] = i;
                    Flag[i] = k;
                }
                while (len > 0) {
                    Pattern[--top] = Pattern[--len];
                }
            }
        }
        D[k] = Y[k];
        Y[k] = 0.0;
        for (; top < n; ++top) {
            const std::size_t i = Pattern[top];
            const double yi = Y[i];
            Y[i] = 0.0;
            const std::size_t l_end = Lp[i] + Lnz[i];
            for (std::size_t p = Lp[i]; p < l_end; ++p) {
                if ((++work & 1023U) == 0U && deadline &&
                    std::chrono::steady_clock::now() >= *deadline) {
                    deadline_reached = true;
                    return false;
                }
                Y[Li[p]] -= Lx[p] * yi;
            }
            const double l_ki = yi / D[i];
            D[k] -= l_ki * yi;
            const std::size_t p_store = Lp[i] + Lnz[i];
            Li[p_store] = k;
            Lx[p_store] = l_ki;
            ++Lnz[i];
        }
        if (std::abs(D[k]) < 1e-15 || !std::isfinite(D[k])) {
            return false;
        }
    }
    return true;
}
}

namespace detail_kkt {
void ldl_lsolve(std::size_t n,
                std::vector<double>& x,
                const std::vector<std::size_t>& Lp,
                const std::vector<std::size_t>& Li,
                const std::vector<double>& Lx) {
    for (std::size_t j = 0; j < n; ++j) {
        const double xj = x[j];
        const std::size_t p2 = Lp[j + 1];
        for (std::size_t p = Lp[j]; p < p2; ++p) {
            x[Li[p]] -= Lx[p] * xj;
        }
    }
}
}

namespace detail_kkt {
void ldl_dsolve(std::size_t n,
                std::vector<double>& x,
                const std::vector<double>& D) {
    for (std::size_t j = 0; j < n; ++j) {
        x[j] /= D[j];
    }
}
}

namespace detail_kkt {
void ldl_ltsolve(std::size_t n,
                 std::vector<double>& x,
                 const std::vector<std::size_t>& Lp,
                 const std::vector<std::size_t>& Li,
                 const std::vector<double>& Lx) {
    for (std::size_t j = n; j > 0; --j) {
        const std::size_t col = j - 1;
        double sum = 0.0;
        const std::size_t p2 = Lp[col + 1];
        for (std::size_t p = Lp[col]; p < p2; ++p) {
            sum += Lx[p] * x[Li[p]];
        }
        x[col] -= sum;
    }
}
}

namespace detail_kkt {
std::uint64_t kkt_pattern_fingerprint(std::size_t total_dim,
                                      const std::vector<std::size_t>& col_ptr,
                                      const std::vector<std::size_t>& row_ind) {
    std::uint64_t h = 1469598103934665603ULL;
    auto mix = [&h](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= (v >> (8 * i)) & 255U;
            h *= 1099511628211ULL;
        }
    };
    mix(total_dim);
    mix(col_ptr.size());
    mix(row_ind.size());
    for (std::size_t value : col_ptr) {
        mix(value);
    }
    for (std::size_t value : row_ind) {
        mix(value);
    }
    return h;
}
}

namespace detail_kkt {
bool same_kkt_pattern(const std::vector<std::size_t>& lhs_col_ptr,
                      const std::vector<std::size_t>& lhs_row_ind,
                      const std::vector<std::size_t>& rhs_col_ptr,
                      const std::vector<std::size_t>& rhs_row_ind) {
    // Exact compare, not a hash compare: a reused symbolic factorization must
    // be provably the analysis of this very pattern.
    return lhs_col_ptr == rhs_col_ptr && lhs_row_ind == rhs_row_ind;
}
}

}
