# W7/D-11: build the markov_cero pybind11 extension.
# The extension links the static markov_cero_core library (built by CMake)
# plus the binding translation unit. No external solver libraries (D-13).
import os
import sys

from setuptools import setup
from setuptools.extension import Extension

try:
    import pybind11
    PYBIND11_INCLUDES = [pybind11.get_include()]
except ImportError:
    PYBIND11_INCLUDES = []

# The CMake build directory holding libmarkov_cero_core.a. Override with
# MARKOV_CERO_BUILD_DIR=<dir> to link a different tree (e.g. build_gap);
# defaults to the historical build_w5.
CORE_BUILD_DIR = os.environ.get("MARKOV_CERO_BUILD_DIR", "build_w5")
CORE_LIB = os.path.join(CORE_BUILD_DIR, "libmarkov_cero_core.a")

ext = Extension(
    "markov_cero._core",
    sources=["python/src/bindings.cpp"],
    include_dirs=[
        "include",
        "gpu/include",
        *PYBIND11_INCLUDES,
    ],
    libraries=["markov_cero_core"],
    library_dirs=[CORE_BUILD_DIR],
    extra_objects=[CORE_LIB] if os.path.exists(CORE_LIB) else [],
    extra_compile_args=["-std=c++20", "-O2", "-fvisibility=hidden"],
    extra_link_args=["-lpthread"],
    language="c++",
)


def build(setup_kwargs):
    """CMake hook entry point (scikit-build style); not used by plain setuptools."""
    setup_kwargs.update({"ext_modules": []})


if __name__ == "__main__":
    setup(
        ext_modules=[ext],
        zip_safe=False,
    )
