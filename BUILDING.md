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

## Packaging qualification, upgrade and rollback

Offline wheel build (no build isolation, no dependency downloads) and the
fresh-venv acceptance run:

```sh
.venv/bin/python -m pip wheel . -w /tmp/opencode/wheels-pkg --no-deps --no-build-isolation
python3 -m venv /tmp/venv-qual
/tmp/venv-qual/bin/python -m pip install pytest numpy   # test-only tools
/tmp/venv-qual/bin/python -m pip install --no-index --no-deps \
  /tmp/opencode/wheels-pkg/markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl
cd /tmp && /tmp/venv-qual/bin/python -m pytest \
  /path/to/markov-initial-build/python/tests -q
```

`pytest` and `numpy` are test-only and are not runtime dependencies of
`markov-cero`; the recorded drill installed them with
`--no-index --find-links <dir>` so the wheel itself stays offline. Without
`numpy`, five tests skip.

Install, upgrade, rollback and uninstall for both the wheel and the CMake
prefix — with the drill transcripts and the exact commands that were executed —
are in [support-rollback](docs/governance/support-rollback.md). Two builds share
the version `0.5.2`, so an upgrade or rollback must use
`pip install --force-reinstall` and must be identified by the wheel SHA-256, not
by the version string. The supported platform matrix, the unassigned support
contact and the still-open release gates (IR-33/G7) are recorded there too.

Release verification is `./scripts/verify-release.sh`; its report is
`evidence/local-verification-report.txt`. Local qualification results for this
round are in
[packaging-qualification-20260928](evidence/packaging-qualification-20260928.json).

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
