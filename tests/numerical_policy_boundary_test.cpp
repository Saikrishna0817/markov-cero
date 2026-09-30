// Contract v1 boundary tests (docs/contracts/numerical-policy.md section 6).
// Every accepted witness is mutated to both sides of the tolerance it is
// checked against: the inner point must verify and the outer point must be
// rejected. No case here converts a failed check into an accepted one.
#include "markov_cero/io/mps.hpp"
#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include "markov_cero/verify/primal_verifier.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace markov_cero;

void req(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        std::exit(1);
    }
}

model::Model linear(const std::string& body) { return io::parse_mps_string(body); }

// Original-model boundary: row feasibility plus integrality.
void original_primal_boundary() {
    const auto m = linear("NAME B\nROWS\n N O\n L R\nCOLUMNS\n X O 1 R 1\nRHS\n R R 1\nENDATA\n");
    const verify::Tolerance tol{1e-9, 1e-9};
    const double allowance = tol.absolute + tol.relative;
    const auto pass = [&](double x) { return verify::verify_primal(m, {{x}, x}, tol, tol).passed; };
    req(pass(1.0), "exact original witness accepted");
    req(pass(1.0 + 0.9 * allowance), "row violation inside tolerance accepted");
    req(!pass(1.0 + 1.1 * allowance), "row violation outside tolerance rejected");

    const auto claimed = [&](double objective) {
        return verify::verify_primal(m, {{1.0}, objective}, tol, tol).passed;
    };
    req(claimed(1.0 + 0.9 * allowance), "objective mismatch inside tolerance accepted");
    req(!claimed(1.0 + 1.1 * allowance), "objective mismatch outside tolerance rejected");

    const auto integer = linear("NAME I\nROWS\n N O\n L R\n"
                                "COLUMNS\n M 'MARKER' 'INTORG'\n X O 1 R 1\n"
                                " M2 'MARKER' 'INTEND'\nRHS\n R R 10\n"
                                "BOUNDS\n LO B X 0\nENDATA\n");
    const double it = 1e-6;
    const auto ipass = [&](double x) {
        return verify::verify_primal(integer, {{x}, x}).passed;
    };
    req(ipass(1.0), "exact integer witness accepted");
    req(ipass(1.0 - 0.9 * it), "integrality violation inside tolerance accepted");
    req(!ipass(1.0 - 1.1 * it), "integrality violation outside tolerance rejected");
}

// Canonical boundary: primal row, dual sign and the duality gap move together.
void canonical_lp_boundary() {
    const auto m = linear("NAME L\nROWS\n N O\n E R\nCOLUMNS\n X O 1 R 1\nRHS\n R R 1\nENDATA\n");
    const auto c = transform::sparse_canonicalize(m);
    req(c.matrix.rows == 1 && c.matrix.columns == 1, "canonical LP is a single row and column");
    const double tol = 1e-8;
    const auto witness = [&](double x) {
        lp::reference::Result r;
        r.status = lp::reference::SolveStatus::optimal;
        r.primal = {x};
        r.dual = {1.0};
        r.objective = x;
        return r;
    };
    req(verify::verify_sparse_result(c, witness(1.0), tol).accepted,
        "exact canonical LP witness accepted");
    req(verify::verify_sparse_result(c, witness(1.0 + 0.5 * tol), tol).accepted,
        "canonical primal violation inside tolerance accepted");
    req(!verify::verify_sparse_result(c, witness(1.0 + 2.0 * tol), tol).accepted,
        "canonical primal violation outside tolerance rejected");
}

qp::QuadraticModel unit_qp(double diagonal) {
    qp::QuadraticModel qm;
    qm.name = "unit_qp";
    qm.P.dimension = 1;
    qm.P.column_offsets = {0, 1};
    qm.P.row_indices = {0};
    qm.P.values = {diagonal};
    qm.q = {0.0};
    qm.A.rows = 0;
    qm.A.columns = 1;
    qm.A.column_offsets = {0, 0};
    return qm;
}

// QP boundary: stationarity residual against the KKT tolerance, and the PSD
// gate that must reject an uncertified curvature before any KKT claim.
void qp_boundary() {
    const double tol = 1e-4;
    const auto kkt = [&](const qp::QuadraticModel& qm, double x) {
        qp::QpSolution s;
        s.status = qp::QpStatus::optimal;
        s.x = {x};
        s.objective_value = 0.5 * x * x;
        return qp::verify_qp_solution(qm, s, tol);
    };
    const auto psd = unit_qp(1.0);
    req(kkt(psd, 0.0).passed, "exact QP KKT witness accepted");
    req(kkt(psd, 0.9 * tol).passed, "QP stationarity residual inside tolerance accepted");
    req(!kkt(psd, 1.1 * tol).passed, "QP stationarity residual outside tolerance rejected");
    req(!kkt(unit_qp(-1.0), 0.0).passed, "curvature that is not certified PSD rejected");
    req(qp::check_convexity(psd.P), "PSD curvature classified positive semidefinite");
}

// Curvature classification: never labels an unknown matrix convex.
void convexity_boundary() {
    qp::SparseSymmetricMatrix indefinite;
    indefinite.dimension = 2;
    indefinite.column_offsets = {0, 1, 2};
    indefinite.row_indices = {0, 1};
    indefinite.values = {1.0, -1.0};
    req(qp::assess_convexity(indefinite).status == qp::ConvexityStatus::non_convex,
        "negative pivot classifies non-convex");

    qp::SparseSymmetricMatrix singular;
    singular.dimension = 2;
    singular.column_offsets = {0, 0, 1};
    singular.row_indices = {0};
    singular.values = {1.0};
    req(qp::assess_convexity(singular).status == qp::ConvexityStatus::indeterminate,
        "zero pivot with an active edge stays indeterminate");
}

// g(x) = x <= 0 with objective slope c: stationarity is c + lambda at x = 0,
// so each dual-sign case isolates the sign check from stationarity.
nlp::NlpModel sign_model(double slope) {
    nlp::NlpModel m;
    m.name = "nlp_dual_sign";
    m.n_vars = 1;
    m.n_ineq = 1;
    m.objective = [slope](const std::vector<double>& x) { return slope * x[0]; };
    m.gradient = [slope](const std::vector<double>&) { return std::vector<double>{slope}; };
    m.ineq_constraints = [](const std::vector<double>& x) { return std::vector<double>{x[0]}; };
    m.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0}};
    };
    m.validate();
    return m;
}

void nlp_boundary() {
    const double tol = 1e-6;
    const auto kkt = [&](double slope, double x, double lambda) {
        nlp::SqpSolution s;
        s.status = lp::reference::SolveStatus::optimal;
        s.x = {x};
        s.objective = slope * x;
        s.ineq_multipliers = {lambda};
        return nlp::verify_nlp_solution(sign_model(slope), s, tol);
    };
    req(kkt(0.0, 0.0, 0.0).accepted, "exact NLP KKT witness accepted");
    req(kkt(0.0, 0.9 * tol, 0.0).accepted, "NLP feasibility violation inside tolerance accepted");
    req(!kkt(0.0, 1.1 * tol, 0.0).accepted, "NLP feasibility violation outside tolerance rejected");
    req(kkt(0.5 * tol, 0.0, -0.5 * tol).accepted, "NLP dual sign inside tolerance accepted");
    req(!kkt(1.5 * tol, 0.0, -1.5 * tol).accepted, "NLP dual sign outside tolerance rejected");
    req(kkt(-2.0, 0.0, 2.0).accepted, "positive inequality multiplier accepted");
}
} // namespace

int main() {
    original_primal_boundary();
    canonical_lp_boundary();
    qp_boundary();
    convexity_boundary();
    nlp_boundary();
    std::cout << "numerical policy boundary tests passed\n";
    return 0;
}
