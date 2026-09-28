#include "bindings_internal.hpp"
#include <cstring>
namespace bindings {
model::Bound make_bound(double value, bool is_lower) {
    if (std::isnan(value) || (is_lower && value == kPyInf) || (!is_lower && value == -kPyInf))
        throw std::invalid_argument("invalid bound: use -inf for a free lower side and +inf for a free upper side");
    if (std::isinf(value)) return is_lower ? model::Bound::negative_infinity()
                                          : model::Bound::positive_infinity();
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


void register_model(py::module_& m) {
    // ---- mc.Model builder -----------------------------------------------------
    py::class_<model::Model>(m, "Model")
        .def(py::init<>())
        .def("continuous_var",
             [](model::Model& self, const std::string& name, double lb, double ub) {
                 const auto lower = make_bound(lb, true), upper = make_bound(ub, false);
                 if (lb > ub) throw std::invalid_argument("lower bound exceeds upper bound");
                 self.variable_name.push_back(name);
                 self.variable_lower.push_back(lower);
                 self.variable_upper.push_back(upper);
                 self.variable_type.push_back(model::VariableType::continuous);
                 grow_matrix_columns(self, self.variable_name.size());
                 return static_cast<int>(self.variable_name.size()) - 1;
             },
             py::arg("name") = "", py::arg("lb") = -kPyInf, py::arg("ub") = kPyInf,
             "Add a continuous variable; returns its column index. Use "
             "math.inf / -math.inf for an unbounded side.")
        .def("integer_var",
             [](model::Model& self, const std::string& name, double lb, double ub) {
                 const auto lower = make_bound(lb, true), upper = make_bound(ub, false);
                 if (lb > ub) throw std::invalid_argument("lower bound exceeds upper bound");
                 self.variable_name.push_back(name);
                 self.variable_lower.push_back(lower);
                 self.variable_upper.push_back(upper);
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
                 const auto lower = make_bound(lb, true), upper = make_bound(ub, false);
                 if (lb > ub) throw std::invalid_argument("lower bound exceeds upper bound");
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
                 self.row_lower.push_back(lower);
                 self.row_upper.push_back(upper);
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


}

} // namespace bindings
