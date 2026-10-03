#include "markov_cero/io/lp_parser.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/gpu/pdhg_step.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/analysis/iis_analyzer.hpp"
#include "markov_cero/api/solve.hpp"
#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <cmath>
#include <chrono>
using namespace markov_cero;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void test_factor_charge_hooks();
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
    test_factor_charge_hooks();
}

namespace {
// IR-21 (contract resource-limits.md §4): a ledger standing in for the
// solve-wide MemoryBudget, so factor-fill charge/release lifecycles are
// observable without a real context.
struct ChargeLedger {
    std::size_t live{0};
    std::size_t peak{0};
    std::size_t charges{0};
    std::size_t releases{0};
    bool refuse{false};
};
bool ledger_charge(void* user, std::size_t bytes) noexcept {
    auto* ledger = static_cast<ChargeLedger*>(user);
    if (ledger->refuse) return false;
    ledger->live += bytes;
    ledger->peak = std::max(ledger->peak, ledger->live);
    ++ledger->charges;
    return true;
}
void ledger_release(void* user, std::size_t bytes) noexcept {
    auto* ledger = static_cast<ChargeLedger*>(user);
    ledger->live = bytes <= ledger->live ? ledger->live - bytes : 0;
    ++ledger->releases;
}
} // namespace

// IR-21: basis-factor fill (linalg, reference simplex, dual session) and the
// QP KKT factor workspace admit bytes into the solve-wide budget while the
// factor lives and hand them back when it dies; a refused charge stops the
// factorization instead of running uncharged.
void test_factor_charge_hooks() {
    const auto csc = linalg::SparseCsc::from_columns(3, {{4, 1, 0}, {1, 5, 1}, {0, 2, 6}});
    linalg::SparseBasisOptions factor_opts;
    factor_opts.charge_bytes = &ledger_charge;
    factor_opts.release_bytes = &ledger_release;

    ChargeLedger factor_ledger;
    factor_opts.charge_user = &factor_ledger;
    {
        auto factor = linalg::SparseBasisFactorization::factorize(csc, factor_opts);
        require(factor_ledger.charges == 1 && factor_ledger.live > 0,
                "factor fill is admitted while the factor lives");
    }
    require(factor_ledger.live == 0 && factor_ledger.releases == 1,
            "the factor charge is released with the factor");

    ChargeLedger refusing;
    refusing.refuse = true;
    factor_opts.charge_user = &refusing;
    bool refused = false;
    try {
        auto factor = linalg::SparseBasisFactorization::factorize(csc, factor_opts);
        (void)factor;
    } catch (const std::length_error& error) {
        refused = true;
        require(std::string(error.what()).find("memory budget") != std::string::npos,
                "the refusal names the solve memory budget");
    }
    require(refused && refusing.charges == 0,
            "a refused factor charge stops the factorization and admits nothing");

    // A cached factor rebinds to the new solve's budget without releasing
    // into the dead one; destruction then releases through the new hooks.
    ChargeLedger first, second;
    factor_opts.charge_user = &first;
    {
        auto cached = linalg::SparseBasisFactorization::factorize(csc, factor_opts);
        const std::size_t factor_bytes = first.live;
        require(factor_bytes > 0, "the first budget holds the factor charge");
        require(cached.rebind_charge(&ledger_charge, &ledger_release, &second),
                "rebind re-admits the cached factor into the new budget");
        require(second.live == factor_bytes, "the new budget holds the full factor bytes");
        require(first.live == factor_bytes, "rebind never releases into the dead budget");
    }
    require(second.live == 0 && second.releases == 1,
            "destruction releases through the new hooks");

    auto charge_model = io::parse_mps_string(
        "NAME CHARGE\nROWS\n N OBJ\n G ONE\nCOLUMNS\n X OBJ 2 ONE 1\n"
        "BOUNDS\n LO B X 3\n UP B X 8\nENDATA\n");
    auto canon = transform::sparse_canonicalize(charge_model, /*relax_integrality=*/true);
    ChargeLedger lp_ledger;
    lp::reference::Options ref_opts;
    ref_opts.charge_bytes = &ledger_charge;
    ref_opts.release_bytes = &ledger_release;
    ref_opts.charge_user = &lp_ledger;
    const auto ref_res = lp::reference::solve(canon, ref_opts);
    require(ref_res.status == lp::reference::SolveStatus::optimal,
            "reference solve of the charge model succeeds");
    require(lp_ledger.charges > 0 && lp_ledger.peak > 0,
            "reference simplex admits basis-factor fill into the budget");
    require(lp_ledger.live == 0, "the basis charge returns when the solve ends");

    ChargeLedger dual_first, dual_second;
    lp::dual::Options dual_opts;
    dual_opts.charge_bytes = &ledger_charge;
    dual_opts.release_bytes = &ledger_release;
    dual_opts.charge_user = &dual_first;
    lp::dual::Session session;
    const auto first_res = session.resolve(canon, dual_opts);
    require(first_res.solution.status == lp::reference::SolveStatus::optimal &&
                dual_first.charges > 0,
            "dual solve admits basis-factor fill into the budget");
    dual_opts.charge_user = &dual_second;
    const auto second_res = session.resolve(canon, dual_opts);
    require(second_res.solution.status == lp::reference::SolveStatus::optimal &&
                dual_second.charges > 0,
            "the second resolve charges the new budget (cached-factor rebind)");
    require(dual_first.live == 0, "the cold solve released its factor with the factor");
    require(dual_second.live > 0, "the session's cached factor holds the second charge");
    const std::size_t cached_bytes = dual_second.live;
    const auto third_res = session.resolve(canon, dual_opts);
    require(third_res.solution.status == lp::reference::SolveStatus::optimal,
            "the third resolve succeeds");
    require(dual_second.live == cached_bytes && dual_second.live > 0,
            "rebinding within the same budget is idempotent (no double charge)");
    session.reset();
    require(dual_second.live == 0, "dropping the cache releases the rebound charge");

    qp::QuadraticModel qm;
    qm.name = "charge_qp";
    qm.P.dimension = 1;
    qm.P.column_offsets = {0, 1};
    qm.P.row_indices = {0};
    qm.P.values = {1.0};
    qm.q = {0.0};
    qm.A.rows = 0;
    qm.A.columns = 1;
    qm.A.column_offsets = {0, 0};
    ChargeLedger qp_ledger;
    qp::QpOptions qp_opts;
    qp_opts.charge_bytes = &ledger_charge;
    qp_opts.release_bytes = &ledger_release;
    qp_opts.charge_user = &qp_ledger;
    const auto qp_res = qp::solve_qp(qm, qp_opts);
    require(qp_res.status == qp::QpStatus::optimal, "charge QP solves optimally");
    require(qp_ledger.charges > 0 && qp_ledger.peak > 0,
            "the KKT factor workspace is admitted into the budget");
    require(qp_ledger.live == 0, "the KKT charge is released when the solve ends");

    ChargeLedger qp_refusing;
    qp_refusing.refuse = true;
    qp_opts.charge_user = &qp_refusing;
    const auto qp_refused = qp::solve_qp(qm, qp_opts);
    require(qp_refused.status == qp::QpStatus::unsupported &&
                std::string(qp_refused.message).find("memory budget") != std::string::npos,
            "a refused KKT charge stops with the budget message");
    require(qp_refusing.charges == 0, "the refused KKT charge admits nothing");
}
