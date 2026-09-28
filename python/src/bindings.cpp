// W7 / D-11: pybind11 Python bindings for markov-cero.
//
// The module exposes EXACTLY the locked API surface (plan §W7.1):
//   mc.solve(path, options=...)          -> SolveResult
//   mc.SolveOptions(...)                 -> engine/threads/backend control
//   mc.Model builder                      -> continuous/integer vars, linear
//                                            objective + constraints, solve()
//   mc.NlpModel(callbacks)               -> SQP path (D-01 Path A)
// No additional surface without owner approval (LOCKED).
//
// Build: compiled against the static markov_cero_core; pybind11 is a
// build-time-only dependency (D-13 sovereignty contract).

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "markov_cero/api/solve.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/minlp/minlp_solver.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;
using namespace markov_cero;

namespace {

constexpr double kPyInf = std::numeric_limits<double>::infinity();

model::Bound make_bound(double value, bool is_lower) {
    // Python convention: +/-inf or NaN on a bound side means "unbounded"
    // (the C++ Bound model distinguishes infinity by kind, not value).
    if (std::isnan(value) || std::isinf(value)) {
        return is_lower ? model::Bound::negative_infinity()
                        : model::Bound::positive_infinity();
    }
    return model::Bound::finite(value);
}

// Keep Model::matrix column-aligned with variable_name: growing a column of
// zeros keeps CSC column_start sized columns+1 for the NEXT add_constraint,
// which reads column_start[j] for every existing column. Zero entries are
// dropped by the builder, so the stored pattern stays canonical.
void grow_matrix_columns(model::Model& self, std::size_t new_cols) {
    const std::size_t rows = self.row_lower.size();
    model::SparseMatrixBuilder builder(rows, new_cols);
    for (std::size_t j = 0; j < self.matrix.column_count && j < new_cols; ++j) {
        for (std::size_t p = self.matrix.column_start[j];
             p < self.matrix.column_start[j + 1]; ++p) {
            builder.add(self.matrix.row_index[p], j, self.matrix.value[p]);
        }
    }
    self.matrix = builder.build();
}

// ---- zero-copy NumPy buffer protocol (feature 23) --------------------------

// Probe once per process whether numpy can be imported. The extension must
// keep working without numpy (zero-runtime-dependency contract): solution
// vectors then fall back to plain Python lists.
bool numpy_available() {
    static const bool available = [] {
        try {
            py::module_::import("numpy");
            return true;
        } catch (const py::error_already_set&) {
            PyErr_Clear();
            return false;
        }
    }();
    return available;
}

// Adopt C++ vector storage as a float64 numpy array WITHOUT copying any
// element: the array borrows the vector's heap buffer and a base capsule
// owns the vector, freeing it when the array is garbage collected. The
// returned array is contiguous and writable (it is now Python-owned).
py::object adopt_vector(std::vector<double>&& values) {
    if (!numpy_available()) {
        return py::cast(values);  // copy into a Python list (fallback only)
    }
    if (values.empty()) {
        py::array arr(py::dtype::of<double>(), py::array::ShapeContainer{0},
                      py::array::StridesContainer{
                          static_cast<py::ssize_t>(sizeof(double))});
        return std::move(arr);
    }
    auto* storage = new std::vector<double>(std::move(values));
    py::capsule owner(storage, [](void* p) {
        delete static_cast<std::vector<double>*>(p);
    });
    const py::ssize_t n = static_cast<py::ssize_t>(storage->size());
    py::array arr(py::dtype::of<double>(), py::array::ShapeContainer{n},
                  py::array::StridesContainer{
                      static_cast<py::ssize_t>(sizeof(double))},
                  storage->data(), owner);
    return std::move(arr);
}

// Read a 1-D float64 buffer (numpy.ndarray, memoryview, array.array, ...)
// straight from its memory — no pybind stl-caster round trip. Strided views
// (e.g. arr[::-1]) are read with their stride; the source is never copied
// twice. Plain Python lists are not buffers and take the stl-caster path.
std::vector<double> read_doubles(const py::object& obj) {
    if (PyObject_CheckBuffer(obj.ptr())) {
        py::buffer buf = py::reinterpret_borrow<py::buffer>(obj.ptr());
        py::buffer_info info = buf.request();
        if (info.format != py::format_descriptor<double>::format()) {
            throw std::invalid_argument(
                "expected a float64 buffer (numpy.ndarray dtype float64, "
                "memoryview, or array.array('d'))");
        }
        if (info.ndim != 1) {
            throw std::invalid_argument("expected a 1-D buffer");
        }
        const auto count = info.shape[0];
        std::vector<double> out(static_cast<std::size_t>(count));
        const auto* base = static_cast<const char*>(info.ptr);
        const auto stride = info.strides[0];
        for (py::ssize_t i = 0; i < count; ++i) {
            out[static_cast<std::size_t>(i)] =
                *reinterpret_cast<const double*>(base + i * stride);
        }
        return out;
    }
    return py::cast<std::vector<double>>(obj);
}

// 2-D float64 buffer read for Jacobian matrices returned by Python
// callbacks; nested Python lists keep the stl-caster path.
std::vector<std::vector<double>> read_matrix(const py::object& obj) {
    if (PyObject_CheckBuffer(obj.ptr())) {
        py::buffer buf = py::reinterpret_borrow<py::buffer>(obj.ptr());
        py::buffer_info info = buf.request();
        if (info.format != py::format_descriptor<double>::format()) {
            throw std::invalid_argument(
                "expected a float64 buffer for the Jacobian matrix");
        }
        if (info.ndim != 2) {
            throw std::invalid_argument(
                "expected a 2-D float64 buffer for the Jacobian matrix");
        }
        const auto rows = info.shape[0];
        const auto cols = info.shape[1];
        const auto* base = static_cast<const char*>(info.ptr);
        const auto row_stride = info.strides[0];
        const auto col_stride = info.strides[1];
        std::vector<std::vector<double>> out(
            static_cast<std::size_t>(rows),
            std::vector<double>(static_cast<std::size_t>(cols)));
        for (py::ssize_t i = 0; i < rows; ++i) {
            for (py::ssize_t j = 0; j < cols; ++j) {
                out[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
                    *reinterpret_cast<const double*>(base + i * row_stride +
                                                     j * col_stride);
            }
        }
        return out;
    }
    return py::cast<std::vector<std::vector<double>>>(obj);
}

// Shared implementation behind mc.solve and Model.solve: run the API on an
// assembled model::Model and translate the result into a Python dict.
py::dict run_solve(const model::Model& model, const api::SolveOptions& options) {
    auto res = api::solve_model(model, options);
    py::dict out;
    out["status"] = std::string(lp::reference::to_string(res.status));
    out["message"] = res.message;
    out["engine"] = res.resolved_engine;
    out["problem_class"] = res.problem_class;
    out["classification_reason"] = res.classification_reason;
    out["objective"] = res.original_objective != 0.0 || !res.original_primal.empty()
                           ? res.original_objective
                           : res.objective;
    // Zero-copy out: move the witness into a float64 array that adopts the
    // vector's storage (no element copy at the boundary).
    std::vector<double> witness;
    if (!res.original_primal.empty()) {
        witness = std::move(res.original_primal);
    } else {
        witness = std::move(res.primal);
    }
    out["x"] = adopt_vector(std::move(witness));
    out["verified"] = res.original_verified || res.canonical_verified;
    out["runtime_ms"] = res.runtime_ms;
    out["nodes"] = res.nodes_explored;
    out["lp_iterations"] = res.lp_iterations;
    out["relative_gap"] = res.relative_gap;
    out["best_bound"] = res.best_bound;
    out["cuts"] = res.cuts_generated;
    py::dict diag;
    diag["primal_residual"] = res.diagnostic.primal_residual;
    diag["dual_residual"] = res.diagnostic.dual_residual;
    diag["failure_site"] = res.diagnostic.failure_site;
    diag["suggested_recovery"] = res.diagnostic.suggested_recovery;
    out["diagnostic"] = diag;
    return out;
}

api::SolveOptions options_from_kwargs(const py::kwargs& kwargs) {
    api::SolveOptions options;
    if (kwargs.contains("engine")) {
        options.engine = py::str(kwargs["engine"]);
    }
    if (kwargs.contains("threads")) {
        options.num_threads = py::int_(kwargs["threads"]);
        options.threads_explicit = true;
    }
    if (kwargs.contains("backend")) {
        options.backend = py::str(kwargs["backend"]);
    }
    if (kwargs.contains("presolve")) {
        options.enable_presolve = py::bool_(kwargs["presolve"]);
    }
    if (kwargs.contains("scale")) {
        options.enable_scale = py::bool_(kwargs["scale"]);
    }
    return options;
}

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

} // namespace

// The compiled extension is shipped as markov_cero._core and re-exported by
// python/markov_cero/__init__.py (a package cannot also be an extension
// module; the underscore submodule is the standard split).
PYBIND11_MODULE(_core, m) {
    m.doc() = "markov-cero: sovereign LP/MILP/QP/MIQP/NLP/MINLP solver core";

    // ---- mc.solve (file path) ------------------------------------------------
    m.def("solve",
          [](const std::string& path, py::kwargs kwargs) {
              // Delegate file parsing to the C++ API (MPS/LP, NLOBJ included).
              // Python contract: an unreadable file raises (zero silent
              // failures, C3) rather than returning a status dict.
              if (!std::filesystem::exists(path)) {
                  // Map to Python FileNotFoundError via OSError with ENOENT.
                  PyErr_SetFromErrnoWithFilename(PyExc_FileNotFoundError, path.c_str());
                  throw py::error_already_set();
              }
              api::SolveOptions options = options_from_kwargs(kwargs);
              auto res = api::solve_file(path, options);
              py::dict out;
              out["status"] = std::string(lp::reference::to_string(res.status));
              out["message"] = res.message;
              out["engine"] = res.resolved_engine;
              out["problem_class"] = res.problem_class;
              out["objective"] = res.original_objective != 0.0 || !res.original_primal.empty()
                                     ? res.original_objective
                                     : res.objective;
              std::vector<double> witness;
              if (!res.original_primal.empty()) {
                  witness = std::move(res.original_primal);
              } else {
                  witness = std::move(res.primal);
              }
              out["x"] = adopt_vector(std::move(witness));
              out["verified"] = res.original_verified || res.canonical_verified;
              out["runtime_ms"] = res.runtime_ms;
              out["nodes"] = res.nodes_explored;
              out["lp_iterations"] = res.lp_iterations;
              out["relative_gap"] = res.relative_gap;
              out["best_bound"] = res.best_bound;
              out["cuts"] = res.cuts_generated;
              return out;
          },
          py::arg("path"),
          "Solve an MPS/LP file. Kwargs: engine, threads, backend, presolve, scale.");

    // ---- mc.SolveOptions (explicit constructor form) -------------------------
    py::class_<api::SolveOptions>(m, "SolveOptions")
        .def(py::init<>())
        .def_readwrite("engine", &api::SolveOptions::engine)
        .def_readwrite("threads", &api::SolveOptions::num_threads)
        .def_readwrite("backend", &api::SolveOptions::backend);

    // ---- mc.Model builder -----------------------------------------------------
    py::class_<model::Model>(m, "Model")
        .def(py::init<>())
        .def("continuous_var",
             [](model::Model& self, const std::string& name, double lb, double ub) {
                 self.variable_name.push_back(name);
                 self.variable_lower.push_back(make_bound(lb, /*is_lower=*/true));
                 self.variable_upper.push_back(make_bound(ub, /*is_lower=*/false));
                 self.variable_type.push_back(model::VariableType::continuous);
                 grow_matrix_columns(self, self.variable_name.size());
                 return static_cast<int>(self.variable_name.size()) - 1;
             },
             py::arg("name") = "", py::arg("lb") = -kPyInf, py::arg("ub") = kPyInf,
             "Add a continuous variable; returns its column index. Use "
             "math.inf / -math.inf (or NaN) for an unbounded side.")
        .def("integer_var",
             [](model::Model& self, const std::string& name, double lb, double ub) {
                 self.variable_name.push_back(name);
                 self.variable_lower.push_back(make_bound(lb, /*is_lower=*/true));
                 self.variable_upper.push_back(make_bound(ub, /*is_lower=*/false));
                 self.variable_type.push_back(model::VariableType::integer);
                 grow_matrix_columns(self, self.variable_name.size());
                 return static_cast<int>(self.variable_name.size()) - 1;
             },
             py::arg("name") = "", py::arg("lb") = 0.0, py::arg("ub") = 1.0,
             "Add an integer variable; returns its column index.")
        .def("binary_var",
             [](model::Model& self, const std::string& name) {
                 self.variable_name.push_back(name);
                 self.variable_lower.push_back(model::Bound::finite(0.0));
                 self.variable_upper.push_back(model::Bound::finite(1.0));
                 self.variable_type.push_back(model::VariableType::binary);
                 grow_matrix_columns(self, self.variable_name.size());
                 return static_cast<int>(self.variable_name.size()) - 1;
             },
             py::arg("name") = "",
             "Add a binary variable; returns its column index.")
        .def("minimize",
             [](model::Model& self, py::object coefficients) {
                 std::vector<double> coeffs = read_doubles(coefficients);
                 self.objective_sense = model::ObjectiveSense::minimize;
                 self.objective = std::move(coeffs);
             },
             py::arg("coefficients"),
             "Set a linear objective (dense coefficient vector) to minimize. "
             "Accepts a float64 buffer (zero-copy read) or a Python list.")
        .def("add_constraint",
             [](model::Model& self, py::object coefficients, double lb,
                double ub, const std::string& name) {
                 // Dense row appended to the CSC matrix. Rebuild from scratch:
                 // models are small on this path; builder semantics are kept
                 // simple and append-only.
                 // Matrix is kept column-aligned by the *_var methods; only a
                 // row append is needed here.
                 const std::vector<double> row = read_doubles(coefficients);
                 const std::size_t rows = self.row_lower.size();
                 const std::size_t cols = self.variable_name.size();
                 if (row.size() != cols) {
                     throw std::invalid_argument("constraint dimension mismatch");
                 }
                 model::SparseMatrixBuilder builder(rows + 1, cols);
                 for (std::size_t j = 0; j < cols; ++j) {
                     for (std::size_t p = self.matrix.column_start[j];
                          p < self.matrix.column_start[j + 1]; ++p) {
                         builder.add(self.matrix.row_index[p], j,
                                     self.matrix.value[p]);
                     }
                 }
                 for (std::size_t j = 0; j < cols; ++j) {
                     if (row[j] != 0.0) {
                         builder.add(rows, j, row[j]);
                     }
                 }
                 self.matrix = builder.build();
                 self.row_lower.push_back(make_bound(lb, /*is_lower=*/true));
                 self.row_upper.push_back(make_bound(ub, /*is_lower=*/false));
                 self.row_name.push_back(name);
             },
             py::arg("coefficients"), py::arg("lb") = -kPyInf, py::arg("ub") = kPyInf,
             py::arg("name") = "",
             "Add a linear row lb <= coefficients . x <= ub. Use math.inf / "
             "-math.inf for one-sided rows.")
        .def("set_nlp_callbacks",
             [](model::Model& self, const nlp::NlpModel& nlp) {
                 self.nlp_callbacks = nlp;
             },
             py::arg("nlp"),
             "Attach programmatic NLP callbacks (D-01 Path A). The model then "
             "classifies as NLP/MINLP (reason 'nlp_callbacks') and solve() "
             "routes it to the sqp/outer_approx engine: the callback objective "
             "adds to the linear objective, callback constraint rows append "
             "after the linear rows, and variable bounds intersect (tightest "
             "finite bound wins); nlp.n_var must equal the variable count.")
        .def("solve",
             [](model::Model& self, py::kwargs kwargs) {
                 return run_solve(self, options_from_kwargs(kwargs));
             },
             "Solve the model with the C++ API. Kwargs: engine, threads, backend.");

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
                 auto sol = nlp::solve_sqp(self, x0, opts);
                 py::dict out;
                 out["status"] = std::string(lp::reference::to_string(sol.status));
                 out["message"] = sol.message;
                 out["objective"] = sol.objective;
                 out["x"] = adopt_vector(std::move(sol.x));
                 out["kkt_residual"] = sol.kkt_residual;
                 out["constraint_violation"] = sol.constraint_violation;
                 out["iterations"] = sol.iterations;
                 out["hessian_resets"] = sol.hessian_resets;
                 return out;
             },
             py::arg("x0"),
             "Solve with SQP (D-02). x0 accepts a float64 buffer (zero-copy "
             "read) or a Python list. The returned x is a zero-copy float64 "
             "array adopting the C++ solution storage. Kwargs: "
             "max_iterations, kkt_tolerance.");
}
