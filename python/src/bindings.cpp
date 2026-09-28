#include "bindings_internal.hpp"
using namespace bindings;
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
                  PyErr_SetString(PyExc_FileNotFoundError, path.c_str());
                  throw py::error_already_set();
              }
              api::SolveOptions options = options_from_kwargs(kwargs);
              return to_python(api::solve_file(path, options));
          },
          py::arg("path"),
          "Solve an MPS/LP file. Kwargs: engine, threads, backend, presolve, scale.");

    // ---- mc.SolveOptions (explicit constructor form) -------------------------
    py::class_<api::SolveOptions>(m, "SolveOptions")
        .def(py::init<>())
        .def_readwrite("engine", &api::SolveOptions::engine)
        .def_property("threads", [](const api::SolveOptions& o) { return o.num_threads; },
            [](api::SolveOptions& o, std::size_t count) {
                if (!count) throw std::invalid_argument("threads must be positive");
                o.num_threads = count; o.threads_explicit = true;
            })
        .def_readwrite("backend", &api::SolveOptions::backend);

    register_model(m);
    register_nlp(m);
}
