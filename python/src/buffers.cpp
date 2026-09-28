#include "bindings_internal.hpp"
#include <cstring>
namespace bindings {
double read_unaligned(const char* p) { double value; std::memcpy(&value, p, sizeof value); return value; }
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
                read_unaligned(base + i * stride);
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
                    read_unaligned(base + i * row_stride + j * col_stride);
            }
        }
        return out;
    }
    return py::cast<std::vector<std::vector<double>>>(obj);
}


} // namespace bindings
