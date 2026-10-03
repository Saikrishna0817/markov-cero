#include "markov_cero/io/lp_parser.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/gpu/pdhg_step.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/analysis/iis_analyzer.hpp"
#include "markov_cero/api/solve.hpp"
#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"
#include <stdexcept>
#include <cmath>
#include <chrono>
using namespace markov_cero;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    auto box = io::parse_mps_string("NAME BOX\nROWS\n N OBJ\nCOLUMNS\n X OBJ 2\nBOUNDS\n LO B X 3\n UP B X 8\nENDATA\n");
    for (bool device : {false, true}) {
        const auto res = device ? gpu::solve_pdlp_gpu(box, {}) : lp::first_order::solve_pdlp(box);
        require(res.status == lp::first_order::PdlpStatus::optimal && res.primal.size() == 1 &&
                std::abs(res.objective - 6) < 1e-12, "box objective and witness");
    }
    box.objective_sense = model::ObjectiveSense::maximize;
    require(std::abs(lp::first_order::solve_pdlp(box).objective - 16) < 1e-12, "max box objective");
    box.variable_upper[0] = model::Bound::positive_infinity();
    require(lp::first_order::solve_pdlp(box).status != lp::first_order::PdlpStatus::optimal, "unbounded box is not optimal");
    qp::QuadraticModel empty;
    empty.objective_offset = 15;
    require(qp::solve_qp(empty).objective_value == 15, "empty QP preserves offset");
    const auto conflict = io::parse_mps_string(
        "NAME CONFLICT\nROWS\n N OBJ\n L LOW\n G HIGH\n L REDUNDANT\nCOLUMNS\n"
        " X LOW 1 HIGH 1\n X REDUNDANT 1\nRHS\n R LOW 0 HIGH 1\n R REDUNDANT 3\nENDATA\n");
    const auto iis = analysis::compute_iis(conflict);
    require(iis.is_infeasible && iis.complete && iis.irreducible_subsystem.size() == 2, "checked row conflict");
    analysis::IisOptions expired;
    expired.deadline = std::chrono::steady_clock::now();
    const auto unknown = analysis::compute_iis(conflict, expired);
    require(!unknown.complete && unknown.diagnostic_summary.find("Inconclusive") != std::string::npos,
            "expired conflict check is inconclusive");
    for (const auto& text : {std::string("Minimize x\nSubject To\n r: x >= 0\nEnd garbage"),
                            std::string(65537, 'x'), std::string("Minimize x\0bad", 14)}) {
        bool rejected = false;
        try { (void)io::parse_lp_string(text); } catch (const std::exception&) { rejected = true; }
        require(rejected, "bounded LP parsing rejects malformed input");
    }

    api::SolveOptions invalid;
    invalid.engine = "unknown";
    require(api::solve_model(box, invalid).status == lp::reference::SolveStatus::invalid_options, "unknown engine rejected");
    invalid.engine = "parallel"; invalid.num_threads = static_cast<std::size_t>(-1);
    require(api::solve_model(box, invalid).status == lp::reference::SolveStatus::invalid_options, "unsafe worker count rejected");
    invalid.num_threads = 1;
    invalid.mip_proof_max_nodes = 0;
    require(api::solve_model(box, invalid).status == lp::reference::SolveStatus::invalid_options,
            "empty proof budget rejected");
    const auto quadratic = io::parse_mps_string("NAME Q\nROWS\n N OBJ\nCOLUMNS\n X OBJ -1\nQUADOBJ\n X X 2\nENDATA\n");
    api::SolveOptions linear; linear.engine = "primal";
    require(api::solve_model(quadratic, linear).status == lp::reference::SolveStatus::unsupported,
            "explicit LP engine must not discard quadratic objective");

    auto max_lp = io::parse_mps_string("NAME MAX\nROWS\n N OBJ\n L CAP\nCOLUMNS\n X OBJ 1 CAP 1\nRHS\n R CAP 1\nBOUNDS\n UP B X 2\nENDATA\n");
    max_lp.objective_sense = model::ObjectiveSense::maximize;
    lp::first_order::PdlpOptions pdlp;
    pdlp.set_tolerance(1e-6);
    auto maximum = gpu::solve_pdlp_gpu(max_lp, pdlp);
    require(maximum.status == lp::first_order::PdlpStatus::optimal && std::abs(maximum.objective-1) < 1e-4,
            "GPU/fallback dual gap preserves maximization sense");
    pdlp.set_tolerance(1e-20); pdlp.max_iterations = 100000000;
    pdlp.deadline = std::chrono::steady_clock::now();
    require(gpu::solve_pdlp_gpu(max_lp, pdlp).status == lp::first_order::PdlpStatus::resource_limit,
            "GPU/fallback observes an expired deadline");

    // IR-20 measured deadline gate (unit layer): an already-expired solve-wide
    // deadline must stop every file-independent engine at the first stage poll,
    // before any engine can claim a result, and the undeadlined control still
    // solves. End-to-end overrun through parse and finalization is measured by
    // scripts/deadline_envelope.py (evidence/deadline-envelope-*.json).
    for (const char* engine : {"primal", "dual", "ipm", "pdlp", "milp", "parallel", "qp"}) {
        api::SolveOptions expired;
        expired.engine = engine;
        expired.total_time_limit_seconds = 1e-12;
        const auto stopped = api::solve_model(box, expired);
        require(stopped.status == lp::reference::SolveStatus::resource_limit,
                "expired solve-wide deadline stops with resource_limit");
        require(stopped.stop_reason == "deadline_exceeded",
                "expired solve-wide deadline is attributed to the wall clock");
        require(!stopped.verified, "an expired deadline never verifies a result");
    }
    api::SolveOptions control;
    control.engine = "primal";
    const auto solved = api::solve_model(box, control);
    require(solved.stop_reason.empty() && solved.status != lp::reference::SolveStatus::resource_limit,
            "undeadlined control takes a non-resource path (no deadline stop)");

    // Standalone SQP polls its own absolute deadline at iteration start.
    nlp::NlpModel ball;
    ball.name = "deadline_box";
    ball.n_vars = 2;
    ball.objective = [](const std::vector<double>& x) { return x[0] * x[0] + x[1] * x[1]; };
    ball.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * x[0], 2.0 * x[1]};
    };
    ball.lower_bounds = {-1.0, -1.0};
    ball.upper_bounds = {1.0, 1.0};
    ball.validate();
    nlp::SqpOptions sqp;
    sqp.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    require(nlp::solve_sqp(ball, {0.5, 0.5}, sqp).status ==
                    lp::reference::SolveStatus::resource_limit,
            "expired SQP deadline stops with resource_limit");
}
