#include "dual_simplex_internal.hpp"
namespace markov_cero::lp::dual {
using namespace detail_dual_simplex;
namespace detail_dual_simplex {
std::size_t select_entering_column(const transform::SparseCanonicalModel& m,
                                   const std::vector<double>& alpha, const std::vector<double>& rc,
                                   const std::vector<bool>& is_basic, const Options& o,
                                   double& best_ratio) {
    std::size_t entering = m.matrix.columns;
    best_ratio = std::numeric_limits<double>::infinity();
    double limit = std::numeric_limits<double>::infinity();
    if (o.harris_ratio) {
        for (std::size_t j = 0; j < m.matrix.columns; ++j) {
            if (is_basic[j] || alpha[j] >= -o.pivot_tolerance) {
                continue;
            }
            const double q = (std::max(0.0, rc[j]) + o.dual_tolerance) / (-alpha[j]);
            if (!std::isfinite(q)) {
                throw std::overflow_error("non-finite Harris ratio");
            }
            limit = std::min(limit, q);
        }
    }
    double best_pivot = 0;
    for (std::size_t j = 0; j < m.matrix.columns; ++j) {
        if (is_basic[j] || alpha[j] >= -o.pivot_tolerance) {
            continue;
        }
        const double q = std::max(0.0, rc[j]) / (-alpha[j]);
        if (!std::isfinite(q)) {
            throw std::overflow_error("non-finite dual ratio");
        }
        if (o.harris_ratio) {
            if (q <= limit && (entering == m.matrix.columns || -alpha[j] > best_pivot ||
                               (-alpha[j] == best_pivot && j < entering))) {
                entering = j;
                best_ratio = q;
                best_pivot = -alpha[j];
            }
        } else if (q < best_ratio || (q == best_ratio && j < entering)) {
            entering = j;
            best_ratio = q;
            best_pivot = -alpha[j];
        }
    }
    return entering;
}
}

namespace detail_dual_simplex {
Result certified_optimal(const transform::SparseCanonicalModel& m, const std::vector<std::size_t>& basis,
                         const std::vector<double>& xb, const std::vector<double>& y,
                         const Options& o) {
    Result out;
    out.solution.status = reference::SolveStatus::optimal;
    out.solution.primal = full_primal(m.matrix.columns, basis, xb);
    out.solution.dual = y;
    out.solution.basis = basis;
    out.solution.objective = dot(m.objective, out.solution.primal) + m.objective_offset;
    out.solution.message = "dual revised simplex optimum";
    try {
        out.basis_state = make_basis_state(m, basis);
    } catch (...) {
        // Degenerate optimal basis: keep the solve result, drop the warm start.
    }
    auto check = verify::verify_sparse_result(
        m, out.solution, std::max(o.feasibility_tolerance, o.dual_tolerance));
    if (!check.accepted) {
        out.solution.status = reference::SolveStatus::numerical_failure;
        out.solution.message = "dual optimum witness rejected: " + check.message;
    } else {
        out.verified = true;
    }
    out.message = out.solution.message;
    return out;
}
}

namespace detail_dual_simplex {
Result certified_farkas(const transform::SparseCanonicalModel& m, const std::vector<double>& pi,
                        const Options& o) {
    Result out;
    out.solution.status = reference::SolveStatus::infeasible;
    out.solution.certificate.resize(m.matrix.rows);
    for (std::size_t i = 0; i < m.matrix.rows; ++i) out.solution.certificate[i] = -pi[i];
    out.solution.message = "dual simplex Farkas certificate";
    auto check = verify::verify_sparse_result(
        m, out.solution, std::max(o.feasibility_tolerance, o.dual_tolerance));
    if (!check.accepted) {
        out.solution.status = reference::SolveStatus::numerical_failure;
        out.solution.message = "dual Farkas witness rejected: " + check.message;
    } else {
        out.verified = true;
    }
    out.message = out.solution.message;
    return out;
}
}

// Basis fingerprint, defined once over the sparse canonical content
// (contract docs/contracts/sparse-lp-path.md §4): dimensions, the
// column-major stream of (column, row, double bits) for every nonzero, and
// the objective coefficients. Dense adapters convert through
// transform::sparse_from_dense first, so both entries hash the same stream
// for equivalent content. Fingerprints written by builds that hashed the
// dense value array no longer match: the warm start is rejected as stale and
// the solve falls back to a cold basis.
std::string fingerprint(const transform::SparseCanonicalModel& m) {
    m.validate();
    std::uint64_t h = 1469598103934665603ULL;
    h = mix(h, m.matrix.rows);
    h = mix(h, m.matrix.columns);
    for (std::size_t j = 0; j < m.matrix.columns; ++j) {
        h = mix(h, j);
        for (std::size_t p = m.matrix.column_offsets[j]; p < m.matrix.column_offsets[j + 1]; ++p) {
            h = mix(h, m.matrix.row_indices[p]);
            h = mix(h, std::bit_cast<std::uint64_t>(m.matrix.values[p]));
        }
    }
    for (double v : m.objective) {
        h = mix(h, std::bit_cast<std::uint64_t>(v));
    }
    return hex(h);
}
std::string fingerprint(const transform::CanonicalModel& m) {
    return fingerprint(transform::sparse_from_dense(m));
}
BasisState make_basis_state(const transform::SparseCanonicalModel& m,
                            const std::vector<std::size_t>& b) {
    BasisState s{m.matrix.rows, m.matrix.columns, fingerprint(m), b};
    validate_basis(m, s);
    return s;
}
BasisState make_basis_state(const transform::CanonicalModel& m, const std::vector<std::size_t>& b) {
    return make_basis_state(transform::sparse_from_dense(m), b);
}
void validate_basis_artifact(const BasisState& s) {
    if (s.rows > maximum_rows || s.columns > maximum_columns || s.rows > s.columns ||
        s.basic_variables.size() != s.rows || s.model_fingerprint.size() != 16) {
        throw std::invalid_argument("invalid basis artifact metadata");
    }
    for (char c : s.model_fingerprint) {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
            throw std::invalid_argument("invalid basis fingerprint");
    }
    std::vector<bool> seen(s.columns);
    for (auto j : s.basic_variables) {
        if (j >= s.columns || seen[j])
            throw std::invalid_argument("basis artifact index invalid or duplicate");
        seen[j] = true;
    }
}
std::string serialize_basis(const BasisState& s) {
    validate_basis_artifact(s);
    std::ostringstream body;
    body << "MARKOV-CERO-BASIS-1 " << s.rows << ' ' << s.columns << ' ' << s.model_fingerprint
         << ' ' << s.basic_variables.size();
    for (auto j : s.basic_variables) body << ' ' << j;
    const auto text = body.str();
    return text + ' ' + hex(hash_text(text)) + "\n";
}
BasisState parse_basis(const std::string& text) {
    std::istringstream in(text);
    std::string magic, fp, checksum, trailing;
    BasisState s;
    std::size_t count = 0;
    if (!(in >> magic >> s.rows >> s.columns >> fp >> count) || magic != "MARKOV-CERO-BASIS-1" ||
        count > maximum_rows) {
        throw std::invalid_argument("invalid basis header");
    }
    s.model_fingerprint = fp;
    s.basic_variables.resize(count);
    for (auto& j : s.basic_variables) {
        if (!(in >> j)) {
            throw std::invalid_argument("truncated basis");
        }
    }
    if (!(in >> checksum) || (in >> trailing)) {
        throw std::invalid_argument("invalid basis trailer");
    }
    std::ostringstream body;
    body << magic << ' ' << s.rows << ' ' << s.columns << ' ' << fp << ' ' << count;
    for (auto j : s.basic_variables) body << ' ' << j;
    if (checksum != hex(hash_text(body.str()))) {
        throw std::invalid_argument("basis checksum mismatch");
    }
    validate_basis_artifact(s);
    return s;
}

namespace detail_dual_simplex {
bool accepted_status(reference::SolveStatus status) {
    return status == reference::SolveStatus::optimal ||
           status == reference::SolveStatus::infeasible ||
           status == reference::SolveStatus::unbounded;
}
}

namespace detail_dual_simplex {
// Verification gate shared by every accepting path (cold, warm and reused
// factorization): an accepted result is only returned when it re-passes
// verify::verify_sparse_result, and `verified` records that fact for tests
// and callers. Results already checked inside certified_optimal/certified_farkas
// are not checked twice.
Result verify_accepted(const transform::SparseCanonicalModel& m, const Options& o, Result out) {
    if (!accepted_status(out.solution.status)) {
        out.verified = false;
        return out;
    }
    if (out.verified) {
        return out;
    }
    const auto check = verify::verify_sparse_result(
        m, out.solution, std::max(o.feasibility_tolerance, o.dual_tolerance));
    out.verified = check.accepted;
    if (!check.accepted) {
        out.solution.status = reference::SolveStatus::numerical_failure;
        out.solution.message = "accepted result rejected by verification: " + check.message;
        out.message = out.solution.message;
    }
    return out;
}
}
}
