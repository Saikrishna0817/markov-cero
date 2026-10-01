#include "nlp_callback_guard.hpp"

#include <cmath>
#include <string>

namespace markov_cero::nlp {

namespace {
thread_local std::size_t g_callback_evaluations = 0;

std::string size_message(const char* what, std::size_t got, std::size_t want) {
    return std::string(what) + " returned " + std::to_string(got) +
           " values, expected " + std::to_string(want);
}
} // namespace

void require_gradient(const NlpModel& model, const std::vector<double>& grad) {
    if (grad.size() != model.n_vars) {
        throw CallbackError(size_message("gradient callback", grad.size(), model.n_vars));
    }
    for (double v : grad) {
        if (!std::isfinite(v)) {
            throw CallbackError("gradient callback returned a non-finite value");
        }
    }
}

void require_objective(double value) {
    if (!std::isfinite(value)) {
        throw CallbackError("objective callback returned a non-finite value");
    }
}

namespace detail {
void note_callback_evaluation() noexcept { ++g_callback_evaluations; }
void reset_callback_evaluations() noexcept { g_callback_evaluations = 0; }
std::size_t take_callback_evaluations() noexcept {
    const std::size_t value = g_callback_evaluations;
    g_callback_evaluations = 0;
    return value;
}
} // namespace detail
} // namespace markov_cero::nlp
