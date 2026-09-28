#include "markov_cero/io/mps.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"
#include <limits>
#include <stdexcept>
#include <string>

using namespace markov_cero;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
model::Model integer_model(const std::string& bounds) {
    return io::parse_mps_string("NAME I\nROWS\n N O\n L R\nCOLUMNS\n M 'MARKER' 'INTORG'\n"
        " X O 1 R 1\n M2 'MARKER' 'INTEND'\nRHS\n R R 10\nBOUNDS\n" + bounds + "ENDATA\n");
}
void parser_cases() {
    require(integer_model("").variable_upper[0].value == 1, "marker default");
    for (const std::string lower : {" LO B X 0\n", " LI B X 0\n", " LI B X 2\n"}) {
        const auto m = integer_model(lower);
        require(!m.variable_upper[0].is_finite(), "lower bound must clear marker upper default");
        require(m.variable_type[0] == model::VariableType::integer, "integer type lost");
    }
    for (const std::string bounds : {" UP B X 5\n LI B X 2\n", " LI B X 2\n UP B X 5\n"}) {
        const auto m = integer_model(bounds);
        require(m.variable_lower[0].value == 2 && m.variable_upper[0].value == 5,
                "explicit upper bound overwritten");
    }
    require(integer_model(" BV B X\n LO B X 0\n").variable_upper[0].value == 1,
            "binary upper is explicit");
    io::MpsLimits limits;
    limits.maximum_line_bytes = 8;
    bool rejected = false;
    try { (void)io::parse_mps_string("NAME toolongname\nENDATA\n", limits); }
    catch (const std::length_error&) { rejected = true; }
    require(rejected, "unbounded input line allocation");
    limits = {};
    limits.maximum_nonzeros = 0;
    rejected = false;
    try { (void)io::parse_mps_string("NAME Q\nROWS\n N O\nCOLUMNS\n X O 0\nQUADOBJ\n X X 1\nENDATA\n", limits); }
    catch (const std::length_error&) { rejected = true; }
    require(rejected, "quadratic terms bypass nonzero limit");
    rejected = false;
    try { (void)io::parse_mps_string("NAME Q\nOBJNAME\n R\nROWS\n N O\n L R\nCOLUMNS\n X R 1\nENDATA\n"); }
    catch (const io::MpsError&) { rejected = true; }
    require(rejected, "OBJNAME must name an N row");
}
void lp_witness_cases() {
    const auto m = io::parse_mps_string("NAME L\nROWS\n N O\n E BIG\n E SMALL\nCOLUMNS\n X O 1 BIG 1\n Y O 1 SMALL 1\nRHS\n R BIG 1e12 SMALL 1\nENDATA\n");
    const auto c = transform::sparse_canonicalize(m);
    lp::reference::Result r;
    r.status = lp::reference::SolveStatus::optimal;
    r.primal = {1e12, 1}; r.dual = {1, 1}; r.objective = 1e12 + 1;
    require(verify::verify_sparse_result(c, r).accepted, "valid LP witness");
    r.primal[1] = 1.01;
    require(!verify::verify_sparse_result(c, r).accepted, "unrelated large RHS weakens local row check");
    r.primal[1] = 1; r.dual.clear();
    require(!verify::verify_sparse_result(c, r).accepted, "missing dual accepted");
    r.dual = {0, 0};
    require(!verify::verify_sparse_result(c, r).accepted, "open duality gap accepted");
    r.status = lp::reference::SolveStatus::infeasible;
    require(!verify::verify_sparse_result(c, r).accepted, "missing Farkas witness accepted");
    r.status = lp::reference::SolveStatus::unbounded;
    require(!verify::verify_sparse_result(c, r).accepted, "missing recession ray accepted");
}
void qp_witness_cases() {
    const auto m = io::parse_mps_string("NAME Q\nROWS\n N O\nCOLUMNS\n X O -1\nBOUNDS\n UP B X 1\nENDATA\n");
    const auto q = qp::make_quadratic_model(m);
    qp::QpSolution s;
    s.x = {1}; s.y = {1}; s.objective_value = -1;
    require(qp::verify_qp_solution(q, s).passed, "valid QP KKT witness");
    s.x = {0}; s.objective_value = 0;
    require(!qp::verify_qp_solution(q, s).passed, "wrong active bound side accepted");
    s.x = {0.5}; s.objective_value = -0.5;
    require(!qp::verify_qp_solution(q, s).passed, "nonzero complementarity accepted");
    s.x = {1}; s.objective_value = -1; s.y.clear();
    require(!qp::verify_qp_solution(q, s).passed, "missing QP dual accepted");
    s.y = {std::numeric_limits<double>::quiet_NaN()};
    require(!qp::verify_qp_solution(q, s).passed, "NaN dual accepted");
    s.y = {1}; s.x[0] = std::numeric_limits<double>::infinity();
    require(!qp::verify_qp_solution(q, s).passed, "infinite primal accepted");
}
int main() { parser_cases(); lp_witness_cases(); qp_witness_cases(); }
