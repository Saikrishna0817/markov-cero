#include "bindings_internal.hpp"
#include "markov_cero/nlp/derivative_check.hpp"
#include <cstring>
namespace bindings {
// NLP objective wrapper: calls the Python callable with a list of floats.
double call_obj(py::object fn, const std::vector<double>& x) {
    py::list lx(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        lx[i] = x[i];
    }
    return py::cast<double>(fn(lx));
}

std::vector<double> call_grad(py::object fn, const std::vector<double>& x) {
    py::list lx(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        lx[i] = x[i];
    }
    return read_doubles(fn(lx));
}

std::vector<double> call_vec(py::object fn, const std::vector<double>& x) {
    py::list lx(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        lx[i] = x[i];
    }
    return read_doubles(fn(lx));
}

std::vector<std::vector<double>> call_mat(py::object fn, const std::vector<double>& x) {
    py::list lx(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        lx[i] = x[i];
    }
    return read_matrix(fn(lx));
}


void register_nlp(py::module_& m) {
    // ---- mc.NlpModel (D-01 Path A callbacks) ----------------------------------
    py::class_<nlp::NlpModel>(m, "NlpModel")
        .def(py::init<>())
        .def_readwrite("n_var", &nlp::NlpModel::n_vars)
        .def("set_objective",
             [](nlp::NlpModel& self, py::object objective, py::object gradient) {
                 self.objective = [objective](const std::vector<double>& x) {
                     return call_obj(objective, x);
                 };
                 self.gradient = [gradient](const std::vector<double>& x) {
                     return call_grad(gradient, x);
                 };
             },
             py::arg("objective"), py::arg("gradient"),
             "Set f(x) and grad f(x) Python callables (both required).")
        .def("add_inequality",
             [](nlp::NlpModel& self, py::object constraint, py::object jacobian) {
                 // Accumulate rows across calls: wrap the running constraint
                 // vector with the new row function.
                 auto prev = self.ineq_constraints;
                 auto prev_jac = self.ineq_jacobian;
                 if (!prev) {
                     self.ineq_constraints =
                         [constraint](const std::vector<double>& x) {
                             return call_vec(constraint, x);
                         };
                     self.ineq_jacobian = [jacobian](const std::vector<double>& x) {
                         return call_mat(jacobian, x);
                     };
                     self.n_ineq += 1;  // row count managed by the user contract
                 } else {
                     self.ineq_constraints =
                         [prev, constraint](const std::vector<double>& x) {
                             auto acc = prev(x);
                             auto row = call_vec(constraint, x);
                             acc.insert(acc.end(), row.begin(), row.end());
                             return acc;
                         };
                     self.ineq_jacobian = [prev_jac, jacobian](const std::vector<double>& x) {
                         auto acc = prev_jac(x);
                         auto rows = call_mat(jacobian, x);
                         acc.insert(acc.end(), rows.begin(), rows.end());
                         return acc;
                     };
                     self.n_ineq += 1;
                 }
             },
             py::arg("constraint"), py::arg("jacobian"),
             "Add g(x) <= 0 rows: constraint returns k values, jacobian k row "
             "vectors.")
        .def("add_equality",
             [](nlp::NlpModel& self, py::object constraint, py::object jacobian) {
                 auto prev = self.eq_constraints;
                 auto prev_jac = self.eq_jacobian;
                 if (!prev) {
                     self.eq_constraints = [constraint](const std::vector<double>& x) {
                         return call_vec(constraint, x);
                     };
                     self.eq_jacobian = [jacobian](const std::vector<double>& x) {
                         return call_mat(jacobian, x);
                     };
                 } else {
                     self.eq_constraints =
                         [prev, constraint](const std::vector<double>& x) {
                             auto acc = prev(x);
                             auto row = call_vec(constraint, x);
                             acc.insert(acc.end(), row.begin(), row.end());
                             return acc;
                         };
                     self.eq_jacobian = [prev_jac, jacobian](const std::vector<double>& x) {
                         auto acc = prev_jac(x);
                         auto rows = call_mat(jacobian, x);
                         acc.insert(acc.end(), rows.begin(), rows.end());
                         return acc;
                     };
                 }
                 self.n_eq += 1;
             },
             py::arg("constraint"), py::arg("jacobian"),
             "Add h(x) = 0 rows (same contract as add_inequality).")
        .def("set_bounds",
             [](nlp::NlpModel& self, py::object lb, py::object ub) {
                 self.lower_bounds = read_doubles(lb);
                 self.upper_bounds = read_doubles(ub);
             },
             py::arg("lb"), py::arg("ub"),
             "Set variable bounds (NaN or +/-math.inf = unbounded side). "
             "Accepts float64 buffers (zero-copy read) or Python lists.")
        .def("solve",
             [](const nlp::NlpModel& self, py::object x0_obj, py::kwargs kwargs) {
                 const std::vector<double> x0 = read_doubles(x0_obj);
                 nlp::SqpOptions opts;
                 if (kwargs.contains("max_iterations")) {
                     opts.max_iterations = py::int_(kwargs["max_iterations"]);
                 }
                  if (kwargs.contains("kkt_tolerance")) {
                      opts.kkt_tolerance = py::float_(kwargs["kkt_tolerance"]);
                  }
                  if (kwargs.contains("elastic_restoration")) {
                      opts.elastic_restoration = py::bool_(kwargs["elastic_restoration"]);
                  }
                 auto sol = nlp::solve_sqp(self, x0, opts);
                 const auto report = nlp::verify_nlp_solution(self, sol, opts.kkt_tolerance);
                 if (sol.status == lp::reference::SolveStatus::optimal)
                     sol.status = report.accepted ? lp::reference::SolveStatus::local_optimal
                                                  : lp::reference::SolveStatus::numerical_failure;
                 py::dict out;
                 out["verified"] = false;
                 out["original_verified"] = report.accepted;
                 out["certificate_type"] = report.accepted ? "local_kkt" : "none";
                 out["status"] = std::string(lp::reference::to_string(sol.status));
                 out["message"] = sol.message;
                 out["objective"] = sol.objective;
                 out["x"] = adopt_vector(std::move(sol.x));
                out["kkt_residual"] = sol.kkt_residual;
                out["constraint_violation"] = sol.constraint_violation;
                // Independent-checker breakdown (nlp-local-sqp.md §7): the
                // bound-normal stationarity residual at the returned point;
                // 0.0 only when the checker rejected before that stage.
                out["stationarity_residual"] = report.stationarity_residual;
                out["complementarity_residual"] = report.complementarity_residual;
                out["iterations"] = sol.iterations;
                out["hessian_resets"] = sol.hessian_resets;
                out["callback_evaluations"] = sol.callback_evaluations;
                out["x0_projection_norm"] = sol.x0_projection_norm;
                // NLP-02 contract nlp-restoration.md section 6.4: elastic
                // restoration observability (accepted / rejected attempts).
                out["restoration_steps"] = sol.restoration_steps;
                out["restoration_failures"] = sol.restoration_failures;
                if (!sol.best_feasible_x.empty()) {
                    auto best = sol.best_feasible_x;
                    out["best_feasible_x"] = adopt_vector(std::move(best));
                    out["best_feasible_objective"] = sol.best_feasible_objective;
                }
                return out;
            },
            py::arg("x0"),
            "Solve with SQP (D-02). x0 accepts a float64 buffer (zero-copy "
            "read) or a Python list. The returned x is a zero-copy float64 "
            "array adopting the C++ solution storage. Kwargs: "
            "max_iterations, kkt_tolerance, elastic_restoration.");
    m.def(
        "check_derivatives",
        [](const nlp::NlpModel& model, py::object x_obj, double step_hint) {
            const std::vector<double> x = read_doubles(x_obj);
            const auto report = nlp::check_derivatives(model, x, step_hint);
            py::dict out;
            out["passed"] = report.passed();
            out["gradient_ok"] = report.gradient_ok;
            out["jacobian_ok"] = report.jacobian_ok;
            out["worst_gradient_error"] = report.worst_gradient_error;
            out["worst_jacobian_error"] = report.worst_jacobian_error;
            out["gradient_checks"] = report.gradient_checks;
            out["jacobian_checks"] = report.jacobian_checks;
            out["message"] = report.message;
            return out;
        },
        py::arg("model"), py::arg("x"), py::arg("step_hint") = 0.0,
        "Developer-only centered/one-sided finite-difference check of the "
        "analytic gradient and constraint Jacobian at x (NLP-01 contract "
        "section 3). Never used inside the solve path.");

}

} // namespace bindings
