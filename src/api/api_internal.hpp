#pragma once
#include "markov_cero/api/solve.hpp"
#include "markov_cero/verify/linear_certificate.hpp"

#include "markov_cero/io/lp_parser.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/minlp/minlp_solver.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/interior/ipm.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/milp/parallel_tree_search.hpp"
#include "markov_cero/presolve/presolve.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/model.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/scale/ruiz_scaling.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>


namespace markov_cero::api::detail {
using Clock = std::chrono::steady_clock;
void run_engine(const model::Model& model, const SolveOptions& options, SolveResult& out, lp::reference::Result& result);
void run_nonlinear(const model::Model& model, const SolveOptions& options, SolveResult& out, lp::reference::Result& result);
void run_parallel(const model::Model& model, const SolveOptions& options, SolveResult& out, lp::reference::Result& result);
void run_pdlp(const model::Model& model, const SolveOptions& options, SolveResult& out, lp::reference::Result& result);
void run_qp(const model::Model& model, const SolveOptions& options, SolveResult& out, lp::reference::Result& result);
void certify_mip(const model::Model&, const SolveOptions&, SolveResult&, lp::reference::Result&);
void run_milp(const model::Model& model, const SolveOptions& options, SolveResult& out, lp::reference::Result& result);
void run_lp(const model::Model& model, const SolveOptions& options, SolveResult& out, lp::reference::Result& result);
std::string format_violation(const verify::PrimalVerificationReport& report);
void fill_complementarity_gap(NumericalDiagnostic&, const std::vector<double>&,
                              const std::vector<double>&, const std::vector<double>&,
                              const std::vector<double>&);
inline bool stop_after_deadline(const SolveOptions& options, SolveResult& out,
                               lp::reference::Result& result, const char* phase) {
    if (!options.lp_options.deadline || Clock::now() < *options.lp_options.deadline) return false;
    result.status = lp::reference::SolveStatus::resource_limit;
    result.message = std::string("wall-clock deadline reached after ") + phase;
    out.diagnostic.failure_site = "api_wall_clock_deadline";
    out.diagnostic.suggested_recovery = "increase_time_limit_or_reduce_preprocessing_cost";
    return true;
}
} // namespace markov_cero::api::detail
