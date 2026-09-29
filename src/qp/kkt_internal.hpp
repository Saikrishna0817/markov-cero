#pragma once
#include "markov_cero/qp/kkt.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>

namespace markov_cero::qp {
namespace detail_kkt {}
namespace detail_kkt {
constexpr std::size_t kMaxKktFactorNonzeros = 10U * 1024U * 1024U;
}
namespace detail_kkt { bool ldl_symbolic(std::size_t n,
                  const std::vector<std::size_t>& Ap,
                  const std::vector<std::size_t>& Ai,
                  std::vector<std::size_t>& Lp,
                  std::vector<std::size_t>& Parent,
                  std::vector<std::size_t>& Lnz,
                  std::vector<std::size_t>& Flag,
                  const std::vector<std::size_t>* P,
                  std::vector<std::size_t>* Pinv,
                  std::optional<std::chrono::steady_clock::time_point> deadline); }
namespace detail_kkt { bool ldl_numeric(std::size_t n,
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
                 bool& deadline_reached); }
namespace detail_kkt { void ldl_lsolve(std::size_t n,
                std::vector<double>& x,
                const std::vector<std::size_t>& Lp,
                const std::vector<std::size_t>& Li,
                const std::vector<double>& Lx); }
namespace detail_kkt { void ldl_dsolve(std::size_t n,
                std::vector<double>& x,
                const std::vector<double>& D); }
namespace detail_kkt { void ldl_ltsolve(std::size_t n,
                 std::vector<double>& x,
                 const std::vector<std::size_t>& Lp,
                 const std::vector<std::size_t>& Li,
                 const std::vector<double>& Lx); }
namespace detail_kkt { std::uint64_t kkt_pattern_fingerprint(std::size_t total_dim,
                                const std::vector<std::size_t>& col_ptr,
                                const std::vector<std::size_t>& row_ind); }
namespace detail_kkt { bool same_kkt_pattern(const std::vector<std::size_t>& lhs_col_ptr,
                   const std::vector<std::size_t>& lhs_row_ind,
                   const std::vector<std::size_t>& rhs_col_ptr,
                   const std::vector<std::size_t>& rhs_row_ind); }
}
