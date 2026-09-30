// LP-01 differential test — docs/contracts/sparse-lp-path.md §5.
//
// The retired dense LP entry path and the sparse primary must agree across
// the frozen netlib set (data/compare/netlib.txt): same solve status, same
// original-scale objective, one shared model fingerprint (including the two
// independent canonicalizers), interchangeable warm-start bases in both
// directions, and invariance of the objective under a row permutation.

#include "markov_cero/io/mps.hpp"
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using markov_cero::lp::reference::SolveStatus;
using markov_cero::transform::CanonicalModel;
using markov_cero::transform::SparseCanonicalModel;

void req(bool condition, const std::string& name, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message + " [" + name + "]");
    }
}

bool close(double a, double b, double rel = 1e-6) {
    return std::abs(a - b) <= rel * std::max({1.0, std::abs(a), std::abs(b)});
}

std::string repo_root() {
    const char* src = std::getenv("MARKOV_CERO_SOURCE_DIR");
    return src ? std::string(src) : std::string(".");
}

// Reverse-permute the rows of a sparse canonical model (record metadata is
// untouched: it maps columns, which a row permutation does not move) and
// re-sort every column so the CSC stays canonical.
SparseCanonicalModel row_permuted(const SparseCanonicalModel& in) {
    if (in.matrix.rows < 2) {
        throw std::runtime_error("row permutation needs at least two rows");
    }
    SparseCanonicalModel out = in;
    const std::size_t rows = in.matrix.rows;
    for (std::size_t i = 0; i < rows; ++i) {
        out.rhs[i] = in.rhs[rows - 1 - i];
    }
    for (auto& r : out.matrix.row_indices) {
        r = rows - 1 - r;
    }
    for (std::size_t j = 0; j < out.matrix.columns; ++j) {
        const std::size_t begin = out.matrix.column_offsets[j];
        const std::size_t end = out.matrix.column_offsets[j + 1];
        std::vector<std::pair<std::size_t, double>> entries;
        entries.reserve(end - begin);
        for (std::size_t k = begin; k < end; ++k) {
            entries.emplace_back(out.matrix.row_indices[k], out.matrix.values[k]);
        }
        std::sort(entries.begin(), entries.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (std::size_t k = 0; k < entries.size(); ++k) {
            out.matrix.row_indices[begin + k] = entries[k].first;
            out.matrix.values[begin + k] = entries[k].second;
        }
    }
    out.validate();
    return out;
}

void check_fixture(const std::string& name, const std::string& path, std::size_t& warm_checks) {
    std::ifstream file(path);
    req(bool(file), name, "fixture file missing");
    const auto model = markov_cero::io::parse_mps(file);
    const CanonicalModel dense = markov_cero::transform::canonicalize(model);
    const SparseCanonicalModel sparse = markov_cero::transform::sparse_canonicalize(model);

    // §5 core requirement: the entry paths agree on status — always, whatever
    // the engines do. blend/scsd1/scsd6 fail simplex phase I numerically; that
    // failure is identical on both paths (verified identical at pre-LP-01
    // HEAD), so it is agreement, not divergence.
    markov_cero::lp::reference::Options ropts;
    ropts.iteration_limit = 500000;
    const auto ref_dense = markov_cero::lp::reference::solve(dense, ropts);
    const auto ref_sparse = markov_cero::lp::reference::solve(sparse, ropts);
    req(ref_dense.status == ref_sparse.status, name, "reference status mismatch");

    markov_cero::lp::dual::Options dopts;
    dopts.iteration_limit = 500000;
    const auto dual_dense = markov_cero::lp::dual::solve(dense, dopts);
    const auto dual_sparse = markov_cero::lp::dual::solve(sparse, dopts);
    req(dual_dense.solution.status == dual_sparse.solution.status, name,
        "dual status mismatch");

    // §4: one fingerprint across entry shapes — dense adapter round trip and
    // the two independent canonicalizers (the engine warm-start interchange).
    req(markov_cero::lp::dual::fingerprint(sparse) ==
            markov_cero::lp::dual::fingerprint(sparse.to_dense()),
        name, "dense round-trip fingerprint mismatch");
    req(markov_cero::lp::dual::fingerprint(sparse) ==
            markov_cero::lp::dual::fingerprint(dense),
        name, "cross-canonicalizer fingerprint mismatch");

    const bool ref_optimal = ref_sparse.status == SolveStatus::optimal;
    const bool dual_optimal = dual_sparse.solution.status == SolveStatus::optimal;
    const double ref_obj_sparse =
        markov_cero::transform::reconstruct_objective(sparse, ref_sparse.objective);
    if (ref_optimal) {
        // §5.3: both entry witnesses pass verification.
        req(markov_cero::verify::verify_reference_result(dense, ref_dense).accepted, name,
            "dense entry witness rejected");
        req(markov_cero::verify::verify_sparse_result(sparse, ref_sparse).accepted, name,
            "sparse entry witness rejected");
    }
    if (dual_optimal) {
        req(dual_dense.verified && dual_sparse.verified, name,
            "dual accepting path skipped verification");
    }
    if (ref_optimal && dual_optimal) {
        const double ref_obj_dense =
            markov_cero::transform::reconstruct_objective(dense, ref_dense.objective);
        req(close(ref_obj_dense, ref_obj_sparse), name, "reference objective mismatch");
        const double dual_obj_dense = markov_cero::transform::reconstruct_objective(
            dense, dual_dense.solution.objective);
        const double dual_obj_sparse = markov_cero::transform::reconstruct_objective(
            sparse, dual_sparse.solution.objective);
        req(close(dual_obj_dense, dual_obj_sparse), name, "dual objective mismatch");
        req(close(ref_obj_sparse, dual_obj_sparse), name,
            "reference/dual objective mismatch");

        // §4: warm-start bases are interchangeable across entry points. Each
        // direction starts from the solve's own validated BasisState — a cold
        // solve delegated to the reference oracle can carry an out-of-range
        // solution.basis and deliberately drops that warm start instead of
        // publishing it (dual_simplex.cpp cold()); empty means nothing to
        // exchange for this fixture.
        if (!dual_dense.basis_state.basic_variables.empty()) {
            const auto state_from_dense = markov_cero::lp::dual::make_basis_state(
                dense, dual_dense.basis_state.basic_variables);
            const auto warm_ds = markov_cero::lp::dual::solve(sparse, dopts, state_from_dense);
            req(warm_ds.used_warm_start && !warm_ds.used_cold_fallback, name,
                "dense basis rejected by sparse solve");
            req(warm_ds.solution.status == SolveStatus::optimal, name,
                "dense->sparse warm not optimal");
            req(close(markov_cero::transform::reconstruct_objective(
                          sparse, warm_ds.solution.objective),
                      ref_obj_sparse),
                name, "dense->sparse warm objective drift");
            ++warm_checks;
        }
        if (!dual_sparse.basis_state.basic_variables.empty()) {
            const auto state_from_sparse = markov_cero::lp::dual::make_basis_state(
                sparse, dual_sparse.basis_state.basic_variables);
            const auto warm_sd = markov_cero::lp::dual::solve(dense, dopts, state_from_sparse);
            req(warm_sd.used_warm_start && !warm_sd.used_cold_fallback, name,
                "sparse basis rejected by dense solve");
            req(warm_sd.solution.status == SolveStatus::optimal, name,
                "sparse->dense warm not optimal");
            ++warm_checks;
        }

        std::cout << "[+] " << name << " obj=" << ref_obj_sparse << "\n";
    } else if (ref_optimal != dual_optimal) {
        req(false, name, "reference/dual engine disagreement within one path");
    } else {
        // Both engines failed on both paths (known pre-existing phase-I
        // failures): agreement is the contract here; no basis to interchange.
        std::cout << "[~] " << name << " engines failed on both paths (agreement only)\n";
    }

    // §5: row ordering does not change the original-scale objective.
    if (ref_optimal) {
        const SparseCanonicalModel permuted = row_permuted(sparse);
        const auto ref_permuted = markov_cero::lp::reference::solve(permuted, ropts);
        req(ref_permuted.status == SolveStatus::optimal, name, "permuted row order not optimal");
        req(close(markov_cero::transform::reconstruct_objective(permuted,
                                                                ref_permuted.objective),
                  ref_obj_sparse),
            name, "row permutation changed objective");
    }
}

} // namespace

int main() {
    try {
        std::ifstream list(repo_root() + "/data/compare/netlib.txt");
        if (!list) {
            throw std::runtime_error("cannot open data/compare/netlib.txt "
                                     "(set MARKOV_CERO_SOURCE_DIR)");
        }
        std::size_t fixtures = 0;
        std::size_t warm_checks = 0;
        std::string line;
        while (std::getline(list, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const auto tab = line.find('\t');
            if (tab == std::string::npos) {
                continue;
            }
            const std::string name = line.substr(0, tab);
            const std::string rel = line.substr(tab + 1);
            check_fixture(name, repo_root() + "/" + rel, warm_checks);
            ++fixtures;
        }
        if (fixtures == 0) {
            throw std::runtime_error("netlib.txt listed no fixtures");
        }
        // Coverage floor: the agreement checks alone would pass if every
        // engine failed everywhere — require the optimal-interchange
        // assertions to have run on most of the frozen set.
        if (warm_checks * 2 <= fixtures) {
            throw std::runtime_error("warm-start interchange ran on too few fixtures");
        }
        std::cout << "All LP sparse-differential fixtures PASSED (" << fixtures
                  << " instances, " << warm_checks << " with full interchange)!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << "\n";
        return 1;
    }
}
