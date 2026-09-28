# Building

Requirements: CMake 3.25+, a C++20 compiler, Python 3.10+, and POSIX shell utilities.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Use `gcc-debug`, `gcc-release`, `clang-debug`, or `clang-release` presets when the matching compiler is installed. Unix Makefiles are the baseline generator; Ninja may be selected explicitly when installed. CUDA is neither required nor detected in M0.

## GLPK comparison baseline on Arch/Omarchy

The W9 comparison calls `glpsol` as a separate process. A system install is
`sudo pacman -S glpk`. When sudo is unavailable, the tested Arch package can be
kept inside the ignored `.venv` directory:

```sh
curl -fL https://stable-mirror.omarchy.org/extra/os/x86_64/glpk-5.0-3-x86_64.pkg.tar.zst \
  -o /tmp/glpk-5.0-3-x86_64.pkg.tar.zst
printf '%s  %s\n' 0ade444b2c7e411eafbb7af75cfed39cd7d686f4eba042aecf08508184e597e4 \
  /tmp/glpk-5.0-3-x86_64.pkg.tar.zst | sha256sum -c -
mkdir -p .venv/local_glpk
bsdtar -xf /tmp/glpk-5.0-3-x86_64.pkg.tar.zst -C .venv/local_glpk
cat > .venv/bin/glpsol <<'EOF'
#!/bin/sh
glpk_root="$(dirname "$0")/../local_glpk/usr"
LD_LIBRARY_PATH="$glpk_root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    exec "$glpk_root/bin/glpsol" "$@"
EOF
chmod +x .venv/bin/glpsol
.venv/bin/glpsol --version
```

The pinned archive checksum matches the `pacman -Sii glpk` repository entry on
the tested host. The solver core never links against GLPK.

## Installed C++ and Python packages

Install the CMake build into a prefix with `cmake --install build --prefix <prefix>`.
Consumers use `find_package(markov_cero CONFIG REQUIRED)` and link
`markov_cero::core`; set `CMAKE_PREFIX_PATH` to that prefix.

`python -m pip wheel .` builds its own CPU static core and Python extension.
It requires a C++20 compiler; the build environment installs CMake and pybind11.
No `build_w5` or `MARKOV_CERO_BUILD_DIR` is used by the wheel build. Install the
wheel before running `python -m pytest python/tests` so an old
in-tree binary cannot hide packaging defects.

Default CTest does not use local virtual environments. Enable comparison tests
with `MARKOV_CERO_ENABLE_BENCHMARK_TESTS=ON` and explicitly supply
`MARKOV_CERO_COMPARE_PYTHON` with HiGHS installed. Enable Python checks separately
with `MARKOV_CERO_ENABLE_PYTHON_TESTS=ON` and set `Python3_EXECUTABLE` to the
interpreter containing the installed package and pytest. An explicit CUDA build
request fails configuration when no CUDA compiler is present.

## Small checkout and optional data

The default build/test suite needs no benchmark download. To restore a selected
optional instance, run `python3 scripts/datasets.py --name gen-ip002`; select a
whole family with `--family netlib`. `--download` explicitly allows fetching the
pinned Git source revision when no local copy exists. Every restored file is
checked against its expected size and SHA-256. `--list` prints the manifest.
Missing optional files produce `DatasetUnavailable` in benchmark reports.
History was not rewritten, so an existing clone can still contain large old objects.

Use `python3 scripts/check_source_limits.py` for the 300-line rule and
`python3 scripts/check_docs.py` for current documentation links. Optional ML
build coverage uses `-DMARKOV_CERO_ENABLE_ML=ON`; no trained model is bundled or
promoted. Supply a standard artifact through `milp::Options::ml_model_path`
(CLI compatibility: `MARKOV_CERO_ML_MODEL`). Training logs are caller-selected
through `strong_branching_log_path` (CLI: `MARKOV_CERO_SB_LOG`).
