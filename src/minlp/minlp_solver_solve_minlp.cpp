#include "minlp_solver_internal.hpp"
namespace markov_cero::minlp {
using namespace detail_minlp_solver;
MinlpSolution solve_minlp(const MinlpProblem& problem, const std::vector<double>& x0,
                          const MinlpOptions& options) {
    MinlpSolution out;
    if (options.max_iterations == 0 || !std::isfinite(options.gap_tolerance) ||
        options.gap_tolerance <= 0.0 || !std::isfinite(options.feasibility_tolerance) ||
        options.feasibility_tolerance <= 0.0 ||
        !std::isfinite(options.sqp_options.kkt_tolerance) ||
        options.sqp_options.kkt_tolerance <= 0.0 || options.milp_max_nodes == 0 ||
        !std::isfinite(options.milp_time_limit) || options.milp_time_limit <= 0.0 ||
        options.max_oa_cuts == 0) {
        out.status = lp::reference::SolveStatus::invalid_options;
        out.message = "minlp: iteration, tolerance, master and OA row limits must be "
                      "positive and finite";
        return out;
    }
    NlpModel nlp = problem.nlp;
    if (!problem.source_model) {
        out.status = lp::reference::SolveStatus::unsupported;
        out.message = "minlp: structurally checkable source model is required for convexity "
                      "screening; arbitrary NLP callbacks are not accepted";
        return out;
    }
    try {
        require_convex_quadratic_structure(*problem.source_model);
        std::vector<std::size_t> expected_integer_indices;
        for (std::size_t j = 0; j < problem.source_model->variable_type.size(); ++j) {
            if (problem.source_model->variable_type[j] != model::VariableType::continuous)
                expected_integer_indices.push_back(j);
        }
        auto supplied_integer_indices = problem.integer_indices;
        std::sort(expected_integer_indices.begin(), expected_integer_indices.end());
        std::sort(supplied_integer_indices.begin(), supplied_integer_indices.end());
        if (std::adjacent_find(supplied_integer_indices.begin(), supplied_integer_indices.end()) !=
            supplied_integer_indices.end()) {
            throw std::invalid_argument("minlp: duplicate integer variable index");
        }
        if (expected_integer_indices != supplied_integer_indices) {
            throw std::invalid_argument(
                "minlp: integer indices must match the source model's discrete variables");
        }
        nlp = io::make_nlp_model(*problem.source_model);
        nlp.validate();
    } catch (const NonConvexMinlp& e) {
        out.status = lp::reference::SolveStatus::non_convex_minlp;
        out.message = e.what();
        return out;
    } catch (const UnsupportedMinlp& e) {
        out.status = lp::reference::SolveStatus::unsupported;
        out.message = e.what();
        return out;
    } catch (const std::exception& e) {
        out.status = lp::reference::SolveStatus::invalid_model;
        out.message = e.what();
        return out;
    }
    if (x0.size() != nlp.n_vars) {
        out.message = "x0 dimension mismatch";
        return out;
    }
    if (std::any_of(x0.begin(), x0.end(), [](double value) { return !std::isfinite(value); })) {
        out.status = lp::reference::SolveStatus::invalid_model;
        out.message = "minlp: x0 contains a non-finite value";
        return out;
    }
    if (nlp.n_eq > 0) {
        out.status = lp::reference::SolveStatus::invalid_model;
        out.message = "minlp: equality constraints are not supported by the OA master; "
                      "reformulate as affine inequalities or use an NLP solver";
        return out;
    }
    for (std::size_t idx : problem.integer_indices) {
        if (idx >= nlp.n_vars) {
            out.message = "integer index out of range";
            return out;
        }
    }

    return iterate_outer_approximation(problem, nlp, x0, options);
}
}
