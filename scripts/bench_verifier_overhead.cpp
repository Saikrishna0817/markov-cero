// Verifier overhead measurement for blueprint task NUM-01 (BENCHMARK REQUIRED):
// time both verification boundaries on a real sparse model and relate them to
// a full production solve of the same model. There is no speed target here;
// the number is recorded so a later tolerance or verifier change can be
// compared against it.
//
//   verifier_overhead_benchmark <model.mps>... [repeats]
//
// solve_ms   wall clock of api::solve_model (production path, which already
//            runs both verifiers internally)
// ref_ms     wall clock of the reference simplex witness solve used below
// canon_us   mean cost of one verify_sparse_result acceptance
// orig_us    mean cost of one verify_primal acceptance
// share_pct  (canon_us + orig_us) / solve_ms as a percentage
#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include "markov_cero/verify/primal_verifier.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace markov_cero;
using Clock = std::chrono::steady_clock;

double milliseconds(Clock::duration elapsed) {
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

model::Model load(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open " + path);
    return io::parse_mps(input);
}

template <typename Fn>
double mean_microseconds(std::size_t repeats, Fn&& fn) {
    fn(); // warm-up so the first call does not own the page faults
    const auto start = Clock::now();
    for (std::size_t i = 0; i < repeats; ++i) fn();
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count() /
           static_cast<double>(repeats);
}

void measure(const std::string& path, std::size_t repeats) {
    const auto model = load(path);
    const auto canonical = transform::sparse_canonicalize(model, /*relax_integrality=*/true);

    const auto solve_start = Clock::now();
    const auto result = api::solve_model(model, {});
    const double solve_ms = milliseconds(Clock::now() - solve_start);

    lp::reference::Options options;
    options.iteration_limit = 500000;
    options.time_limit_seconds = 60.0;
    const auto reference_start = Clock::now();
    const auto witness = lp::reference::solve(canonical, options);
    const double reference_ms = milliseconds(Clock::now() - reference_start);

    std::cout << path << " rows=" << model.matrix.row_count << " cols=" << model.matrix.column_count
              << " nnz=" << model.matrix.value.size() << " status="
              << lp::reference::to_string(result.status) << " assurance=" << result.assurance
              << std::fixed << std::setprecision(3) << " solve_ms=" << solve_ms
              << " ref_solve_ms=" << reference_ms << "\n";

    if (witness.status != lp::reference::SolveStatus::optimal) {
        std::cout << "  reference witness unavailable ("
                  << lp::reference::to_string(witness.status) << "); verification not measured\n";
        return;
    }

    bool canonical_accepted = false;
    const double canon_us = mean_microseconds(repeats, [&] {
        canonical_accepted = verify::verify_sparse_result(canonical, witness).accepted;
    });

    const verify::Candidate candidate{transform::reconstruct_primal(canonical, witness.primal),
                                      transform::reconstruct_objective(canonical, witness.objective)};
    // The reference solve returns an LP relaxation of an integer model, so the
    // original-model check on such a file drops the integrality requirement
    // instead of rejecting a fractional relaxation witness it never claimed.
    const bool has_integer =
        std::any_of(model.variable_type.begin(), model.variable_type.end(),
                    [](model::VariableType type) { return type != model::VariableType::continuous; });
    bool original_accepted = false;
    const double original_us = mean_microseconds(repeats, [&] {
        original_accepted =
            verify::verify_primal(model, candidate, {}, {}, 1e-6, !has_integer).passed;
    });

    const double share = 100.0 * (canon_us + original_us) / 1000.0 / solve_ms;
    std::cout << "  repeats=" << repeats << " canonical=" << (canonical_accepted ? "accepted" : "REJECTED")
              << " original=" << (original_accepted ? "accepted" : "REJECTED")
              << (has_integer ? " (integrality relaxed: LP relaxation witness)" : "")
              << std::setprecision(2) << " canon_us=" << canon_us << " orig_us=" << original_us
              << " share_pct=" << share << "\n";
    if (!canonical_accepted || !original_accepted) {
        std::cout << "  WARNING: a reference-solver witness was rejected; this model is not usable "
                     "for the overhead ratio\n";
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: verifier_overhead_benchmark <model.mps>... [repeats]\n";
        return 2;
    }
    std::size_t repeats = 32;
    std::vector<std::string> models;
    for (int i = 1; i < argc; ++i) models.push_back(argv[i]);
    // An optional trailing all-digit argument is the repeat count.
    const std::string last = models.back();
    if (models.size() > 1 && !last.empty() &&
        last.find_first_not_of("0123456789") == std::string::npos) {
        repeats = static_cast<std::size_t>(std::stoul(last));
        models.pop_back();
    }
    try {
        for (const auto& path : models) measure(path, repeats);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
