// MINLP-02 contract §7 (docs/contracts/minlp-proof-replay.md): one mutation
// per proof field plus seeded count/fingerprint/version fuzz. The control
// proof is accepted first and last; every tampered variant must be rejected
// with a message naming the obligation. Zero forged proofs accepted.

#include "minlp02_common.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace markov_cero;
using minlp02::contains;
using minlp02::require;

namespace {
void expect_rejected(const model::Model& source, const verify::OaProof& proof,
                     const char* label, const char* obligation) {
    const auto report = verify::verify_oa_proof(source, proof);
    if (report.accepted)
        throw std::runtime_error(std::string("accepted forged proof: ") + label);
    if (report.tier != verify::OaAssuranceTier::unverified)
        throw std::runtime_error(std::string("forged proof not unverified: ") + label);
    if (!contains(report.message, obligation))
        throw std::runtime_error(std::string("message lacks ") + obligation + " for " +
                                 label + ": " + report.message);
}

// Seeded LCG; fuzz mutations touch only version, fingerprint and count tokens.
struct Lcg {
    std::uint64_t state;
    explicit Lcg(std::uint64_t seed) : state(seed) {}
    std::uint32_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(state >> 33);
    }
};

// Serializes the proof, then mutates exactly one structural token: the header
// version digit, a fingerprint digit or the leading digit of one count token
// (fingerprint length, pivot count, cut count, history count, incumbent count,
// embedded byte length). Claim lines are never touched.
std::string fuzz_mutate(const std::string& text, Lcg& rng, std::size_t ncuts,
                        std::size_t nhist) {
    std::string out = text;
    const std::size_t pick = rng.next() % 4;
    if (pick == 0) {
        out[16] = static_cast<char>('0' + rng.next() % 10); // version digit
        if (out[16] == '1') out[16] = '2';
        return out;
    }
    std::vector<std::string> lines;
    std::vector<std::size_t> starts;
    std::size_t cursor = 0;
    while (cursor < out.size()) {
        const auto nl = out.find('\n', cursor);
        if (nl == std::string::npos) break;
        starts.push_back(cursor);
        lines.push_back(out.substr(cursor, nl - cursor));
        cursor = nl + 1;
        if (lines.size() == 9 + ncuts + nhist) break; // byte-length line reached
    }
    // Count token line indexes: fp length (2), pivots (4), cuts (5),
    // history (6+ncuts), incumbent (7+ncuts+nhist), bytes (8+ncuts+nhist).
    const std::vector<std::size_t> count_lines = {
        2, 4, 5, 6 + ncuts, 7 + ncuts + nhist, 8 + ncuts + nhist};
    const std::size_t line_index =
        pick == 1 ? 2 : count_lines[rng.next() % count_lines.size()];
    if (line_index >= starts.size()) return out;
    const std::size_t start = starts[line_index];
    if (pick == 1) {
        // Fingerprint digit: first digit after the length token on line 2.
        const auto space = out.find(' ', start);
        const std::size_t at = space + 1 + rng.next() % 19;
        if (at < out.size() && out[at] >= '0' && out[at] <= '9')
            out[at] = static_cast<char>('0' + (out[at] - '0' + 1 + rng.next() % 9) % 10);
        return out;
    }
    if (start < out.size() && out[start] >= '0' && out[start] <= '9')
        out[start] = static_cast<char>('0' + (out[start] - '0' + 1 + rng.next() % 9) % 10);
    return out;
}
} // namespace

int main() {
    const auto source = minlp02::case_a_source();
    const auto res = api::solve_model(source, {});
    require(res.oa_proof != nullptr, "control proof attached");
    const verify::OaProof control = *res.oa_proof;
    require(verify::verify_oa_proof(source, control).accepted,
            "control proof accepted first");

    // One mutation per proof field (contract §7 list, in order).
    { auto p = control; p.model_fingerprint = "9999999999999999999";
      expect_rejected(source, p, "stale fingerprint", "o2 model fingerprint"); }
    { auto p = control; p.cuts.pop_back();
      expect_rejected(source, p, "missing tangent", "o5 master assembly fingerprint"); }
    { auto p = control; p.cuts[0].gradient[0] += 1e-3;
      expect_rejected(source, p, "altered gradient", "o4 tangent replay"); }
    { auto p = control; p.cuts[0].point[0] += 0.1;
      expect_rejected(source, p, "altered point", "o4 tangent replay"); }
    { auto p = control; p.cuts[0].rhs += 0.05;
      expect_rejected(source, p, "altered rhs", "o4 tangent replay"); }
    { auto p = control; p.cuts[0].weakening = -1.0;
      expect_rejected(source, p, "negative weakening", "o4 tangent replay"); }
    { auto p = control;
      for (auto& cut : p.cuts)
          if (cut.source_kind == minlp::OaCutSource::nlcon) { cut.source_index = 9; break; }
      expect_rejected(source, p, "out-of-range source index", "o4 tangent replay"); }
    { auto p = control; p.cuts.push_back(p.cuts[0]);
      expect_rejected(source, p, "duplicated tangent", "o5 master assembly fingerprint"); }
    { auto p = control; p.master_proof.nodes.pop_back();
      expect_rejected(source, p, "truncated embedded tree", "o6 master tree"); }
    { auto p = control; p.claimed_best_bound += 0.05;
      expect_rejected(source, p, "forged claimed bound", "o7 claimed bound"); }
    { auto p = control; p.incumbent[1] += 0.5;
      expect_rejected(source, p, "non-integral incumbent", "o8 incumbent is not integral"); }
    { auto p = control; p.incumbent[0] = 5.0;
      expect_rejected(source, p, "infeasible incumbent",
                      "o8 incumbent fails source feasibility"); }
    { auto p = control; p.claimed_objective += 0.1;
      expect_rejected(source, p, "forged incumbent objective", "o8 incumbent objective"); }
    { auto p = control; p.source_sense = verify::OaProofSense::maximize;
      expect_rejected(source, p, "flipped source sense", "o2 source sense"); }
    { auto p = control; p.convexity_pivots[0] += 0.5;
      expect_rejected(source, p, "forged convexity pivot", "o3 convexity evidence mismatch"); }
    { auto p = control;
      p.master_history[0].bound = p.master_history.back().bound + 1.0;
      expect_rejected(source, p, "non-monotone history",
                      "o7 certified bound history is not monotone"); }
    { auto p = control; p.kind = verify::OaProofKind::infeasible;
      expect_rejected(source, p, "kind/claims inconsistency",
                      "o10 infeasible claim without"); }
    { auto p = control;
      p.claimed_objective = std::numeric_limits<double>::quiet_NaN();
      expect_rejected(source, p, "NaN claim injection", "o1 non-finite claim"); }

    // Seeded fuzz (fixed seed): mutate only version, fingerprint and count
    // tokens. A parse failure is a rejection; a parseable mutant must never
    // verify as accepted at the independent_oa tier.
    const std::string text = minlp02::serialize(control);
    Lcg rng(20261001);
    std::size_t parsed = 0;
    for (int iter = 0; iter < 64; ++iter) {
        const std::string mutant =
            fuzz_mutate(text, rng, control.cuts.size(), control.master_history.size());
        std::istringstream in(mutant);
        verify::OaProof proof;
        try {
            proof = verify::read_oa_proof(in);
        } catch (const std::exception&) {
            continue;
        }
        ++parsed;
        const auto report = verify::verify_oa_proof(source, proof);
        if (report.accepted && report.tier == verify::OaAssuranceTier::independent_oa)
            throw std::runtime_error("fuzz produced an accepted independent_oa proof at " +
                                     std::to_string(iter));
    }
    require(parsed > 0, "fuzz produced at least one parseable mutant");
    require(verify::verify_oa_proof(source, control).accepted,
            "control proof accepted last");

    std::cout << "MINLP-02 proof attacks and seeded fuzz passed (" << parsed
              << " parseable mutants rejected)\n";
}
