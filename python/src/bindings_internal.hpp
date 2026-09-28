#pragma once
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
namespace bindings {
constexpr double kPyInf = std::numeric_limits<double>::infinity();
py::object adopt_vector(std::vector<double>&&);
std::vector<double> read_doubles(const py::object&);
std::vector<std::vector<double>> read_matrix(const py::object&);
py::dict to_python(api::SolveResult);
py::dict run_solve(const model::Model&, const api::SolveOptions&);
api::SolveOptions options_from_kwargs(const py::kwargs&);
void register_model(py::module_&);
void register_nlp(py::module_&);
}
