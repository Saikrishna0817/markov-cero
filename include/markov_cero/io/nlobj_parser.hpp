#pragma once

// W1 / D-01 Path B / D-12 / D-20: bridge from a parsed model::Model carrying
// NLOBJ terms to an nlp::NlpModel for the SQP engine. Also composes Path A
// callback companions (model.nlp_callbacks) when present.

#include "markov_cero/model/model.hpp"
#include "markov_cero/nlp/nlp_model.hpp"

#include <limits>

namespace markov_cero::io {

/// Build the SQP-consumable NLP model from a parsed model (linear constraints
/// ride as <= callbacks; the objective needs NLOBJ terms or an attached
/// model.nlp_callbacks companion — Path A). When model.nlp_callbacks is set
/// it is composed in: objective added, g/h rows appended, bounds intersected
/// (throws std::invalid_argument on a malformed companion).
[[nodiscard]] nlp::NlpModel make_nlp_model(const model::Model& model);

/// Convenience wrapper holding the source model and constructing the NLP view
/// on demand (keeps api.cpp tidy).
struct NlobjBridge {
    const model::Model& source;

    [[nodiscard]] nlp::NlpModel build() const { return make_nlp_model(source); }
};

} // namespace markov_cero::io
