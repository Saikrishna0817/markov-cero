// W1 gate: NLOBJ MPS section parsing (D-01 Path B, D-12, D-20).
#include "markov_cero/io/mps.hpp"
#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/model/classifier.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

const char* kModel = R"(NAME NLOBJDEMO
ROWS
 N COST
 G R1
COLUMNS
    X1  COST  1.0  R1  1.0
    X2  COST  1.0  R1  1.0
RHS
    RHS1  R1  3.0
BOUNDS
 UP BND  X1  10.0
 UP BND  X2  10.0
NLOBJ
* quadratic penalty term 0.5 x1^2
  0.5  X1  X1
* bilinear term -0.25 x1 x2
  -0.25  X1  X2
ENDATA
)";

} // namespace

int main() {
    std::istringstream input(kModel);
    const auto model = io::parse_mps(input);
    req(model.has_nlobj_section, "NLOBJ section detected");
    req(model.nlobj_terms.size() == 2, "two NLOBJ terms parsed");
    req(std::abs(model.nlobj_terms[0].coefficient - 0.5) < 1e-12, "first coefficient");
    req(model.nlobj_terms[0].quadratic, "x1^2 marked quadratic");
    req(model.nlobj_terms[0].var0 == 0 && model.nlobj_terms[0].var1 == 0, "x1 indices");
    req(model.nlobj_terms[1].quadratic, "bilinear marked quadratic");
    req(model.nlobj_terms[1].var0 == 0 && model.nlobj_terms[1].var1 == 1, "bilinear indices");

    // Comment lines (leading *) are skipped by the tokenizer; a degree-1 term
    // parses as a linear NLOBJ term.
    const char* kLinear = R"(NAME NLIN
ROWS
 N COST
COLUMNS
    X1  COST  1.0
RHS
BOUNDS
 UP BND  X1  10.0
NLOBJ
  2.0  X1
ENDATA
)";
    std::istringstream lin(kLinear);
    const auto m2 = io::parse_mps(lin);
    req(m2.has_nlobj_section && m2.nlobj_terms.size() == 1, "degree-1 term parsed");
    req(!m2.nlobj_terms[0].quadratic, "degree-1 not quadratic");

    // Classifier: NLOBJ + continuous -> NLP (locked tree Path B).
    model::ClassificationInputs in;
    in.has_nlobj_section = true;
    const auto cls = model::classify_model(model, in);
    req(cls.problem_class == model::ProblemClass::nlp, "NLOBJ classifies as NLP");

    // Bridge to the SQP engine and evaluate the objective at a known point.
    const auto nlp = io::make_nlp_model(model);
    req(nlp.n_vars == 2, "bridge variable count");
    const std::vector<double> x{2.0, 1.0};
    // f = (1*x1 + 1*x2) + 0.5*x1*x1 - 0.25*x1*x2 = 3 + 2 - 0.5 = 4.5
    const double f = nlp.eval_objective(x);
    req(std::abs(f - 4.5) < 1e-12, "bridge objective value");
    const auto g = nlp.eval_gradient(x);
    // df/dx1 = 1 + x1 - 0.25*x2 = 2.75 ; df/dx2 = 1 - 0.25*x1 = 0.5
    req(std::abs(g[0] - 2.75) < 1e-12, "bridge gradient x1");
    req(std::abs(g[1] - 0.5) < 1e-12, "bridge gradient x2");

    const char* kNlcon = R"(NAME NLCONDEMO
ROWS
 N COST
COLUMNS
    X1  COST  1.0
    X2  COST  1.0
NLCON
  0.25 X1 X1 <= 0.5 nl1
  -0.5 X1 <= 0.5 nl1
  1.0 X2 <= 2.0
ENDATA
)";
    std::istringstream ncin(kNlcon);
    const auto ncm = io::parse_mps(ncin);
    req(ncm.has_nlobj_section, "NLCON content activates nonlinear classification");
    req(ncm.nlcon_constraints.size() == 2, "named NLCON terms merge; unnamed row gets a name");
    req(ncm.nlcon_constraints[0].terms.size() == 2, "named NLCON terms accumulate");
    req(ncm.nlcon_constraints[1].name == "nlcon_1", "unnamed NLCON auto-name");
    const auto ncnlp = io::make_nlp_model(ncm);
    req(ncnlp.n_ineq == 2, "NLCON rows appended to nonlinear bridge");
    const auto ncv = ncnlp.ineq_constraints({2.0, 1.0});
    const auto ncj = ncnlp.ineq_jacobian({2.0, 1.0});
    req(std::abs(ncv[0] + 0.5) < 1e-12, "merged NLCON value and RHS");
    req(std::abs(ncj[0][0] - 0.5) < 1e-12, "merged NLCON gradient");
    req(std::abs(ncv[1] + 1.0) < 1e-12, "unnamed NLCON value");

    std::cout << "nlobj parser tests passed\n";
    return 0;
}
