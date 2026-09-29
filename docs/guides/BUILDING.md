# Building and installing markov-cero

Run commands from the **repository root**. The normal build needs CMake 3.25 or newer, a C++20 compiler and POSIX shell utilities. Python 3.10 or newer runs repository scripts; Python package builds follow `pyproject.toml` and need pybind11. CPU builds do not need a CUDA toolkit or an external solver library.

## Native CPU build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

`MARKOV_CERO_WARNINGS_AS_ERRORS` is on by default. Use a separate build directory for each compiler, sanitizer or CUDA configuration. The build defines `markov_cero_core` and CLI tools including `markov-cero-solve`, `markov-cero-verify-mip`, `markov-cero-iis`, `markov-cero-mps-inspect` and `markov-cero-info`. `BUILD_TESTING=OFF` omits CTest targets for a lean consumer build.

The default tests use small tracked fixtures. Set `MARKOV_CERO_ENABLE_BENCHMARK_TESTS=ON` only when you have the required comparator interpreter and optional benchmark data; supply `MARKOV_CERO_COMPARE_PYTHON` with HiGHS installed. `MARKOV_CERO_ENABLE_PYTHON_TESTS=ON` checks a **separately installed** Python extension and requires an appropriate `Python3_EXECUTABLE`.

## Install the C++ library

```sh
cmake --install build --prefix /tmp/markov-cero-prefix
```

A CMake consumer can set `CMAKE_PREFIX_PATH=/tmp/markov-cero-prefix`, call `find_package(markov_cero CONFIG REQUIRED)` and link `markov_cero::core`. The install includes public headers from `include/markov_cero/` and the optional GPU API headers from `gpu/include/markov_cero/`, plus license, notice and provenance records. The [public API](../../include/markov_cero/api/solve.hpp) defines solve options and results.

## Python wheel

```sh
python3 -m pip wheel . --no-deps -w /tmp/markov-wheels
```

`setup.py` invokes CMake to build an isolated CPU core for the extension, then links the pybind11 module. It does not reuse a previous local `build/` tree. Build requirements are in `pyproject.toml`. Install the wheel in a fresh environment before running `python -m pytest python/tests`; an in-tree compiled module can otherwise mask a packaging error. A dated offline wheel/prefix/rollback qualification is in [evidence](../../evidence/packaging-qualification-20260928.json). That record is not a fresh result from this checkout.

## Optional CUDA and ML paths

Set `-DMARKOV_CERO_ENABLE_CUDA=ON` only when a compatible CUDA compiler and toolkit are available. CMake fails explicitly if requested CUDA support cannot be configured. The CPU backend remains the default. GPU correctness evidence exists, while measured end-to-end speed benefit remains open; see the [host record](../../evidence/gpu_hardware_host_access_check_20260928.json).

`MARKOV_CERO_ENABLE_ML=ON` compiles the optional ONNX branching scorer. It does not provide a promoted runtime model. The caller must supply a model path, and ML-assisted branching remains an experimental, explicit opt-in path. See [status](../project/STATUS.md).

## Dataset and comparison runs

The default checkout retains small offline fixtures. The [optional dataset manifest](../../data/optional-datasets.json) lists larger Netlib, MIPLIB, Mittelmann and QPLIB instances with expected hashes. Restore a selected instance with:

```sh
python3 scripts/datasets.py --list
python3 scripts/datasets.py --name gen-ip002
```

A local cache or pinned Git revision supplies the file when available; `--download` explicitly permits fetching it when absent. Missing optional inputs are reported as unavailable rather than silently dropped from benchmark denominators. Comparison tools under `scripts/` use external solver **processes** as oracles and do not link them into the production core. Read the [evidence index](../../evidence/INDEX.md) before comparing a new result with a historical run.

## Release checks and known packaging boundary

Run `bash scripts/verify-release.sh` for the dual-compiler local release check. It writes an ignored local report; [verification](VERIFY.md) explains what that report establishes. Packaging and support gates remain open in the [gate register](../../evidence/gate-status-20260929.json). The checked-in `LICENSE` is Apache 2.0, while `pyproject.toml` currently labels the package Proprietary; resolve that metadata discrepancy before publishing a wheel.
