#pragma once

// MINLP-01 (docs/contracts/minlp-oa.md §4-§5): provenance record for one
// outer-approximation tangent and independent replay of that record from the
// source polynomial. Creation evaluates the make_nlp_model callback view;
// replay recomputes from the source model directly and never trusts the
// stored coefficients. MINLP-02 builds the exported OA proof format on the
// same two functions.

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace markov_cero::minlp {

// Outward guard added to every cut rhs (contract §4.2) and the componentwise
// replay tolerance (contract §5.3), both fixed by the contract before code.
inline constexpr double kOaCutWeakening = 1e-9;
inline constexpr double kOaCutReplayTolerance = 1e-8;

// source_index placeholder for objective cuts (contract §4.1).
inline constexpr std::size_t kOaObjectiveSource = static_cast<std::size_t>(-1);

enum class OaCutSource { objective, linear_upper, linear_lower, nlcon };

// Row form everywhere: gradient^T x <= rhs with
// rhs = gradient^T point - value + weakening (contract §4.1).
struct OaCut {
    OaCutSource source_kind{OaCutSource::objective};
    std::size_t source_index{kOaObjectiveSource};
    bool source_maximize{false};
    std::vector<double> point;
    std::vector<double> gradient;
    double value{0.0};
    double rhs{0.0};
    double weakening{0.0};
};

struct OaReplayReport {
    bool accepted{false};
    std::size_t checked{0};
    std::string message;
};

// Source-provenance of one NLP inequality row: row order matches
// make_nlp_model (per matrix row upper then lower, then NLCON in order).
struct OaRowSource {
    OaCutSource kind{OaCutSource::nlcon};
    std::size_t index{0};
};

// Build a cut directly from the source polynomial at `point` (contract §5.4).
// Throws std::invalid_argument on a malformed kind/index or wrong dimension.
[[nodiscard]] OaCut derive_oa_cut(const model::Model& source, OaCutSource kind,
                                  std::size_t source_index,
                                  const std::vector<double>& point);

// Recompute every cut from the source polynomial and compare against the
// stored record (contract §5.3); first mismatch stops with accepted=false.
[[nodiscard]] OaReplayReport replay_oa_cuts(const model::Model& source,
                                            const std::vector<OaCut>& cuts);

// Map NLP inequality row index -> source provenance (contract §5.1).
[[nodiscard]] std::vector<OaRowSource> oa_ineq_source_map(const model::Model& source);

} // namespace markov_cero::minlp
