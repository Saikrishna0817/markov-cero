# Quickstart

This guide takes a new contributor from a clean checkout to a local solve. Run commands from the **repository root**. You need CMake 3.25 or newer, a C++20 compiler, Python 3.10 or newer for the repository scripts, and a POSIX shell.

## Build and run a small LP

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/markov-cero-solve examples/blend.mps --output /tmp/markov-blend.json
cat /tmp/markov-blend.json
```

The CLI selects an engine when `--engine auto` is in effect. Its JSON includes the resolved engine, status, objective or best bound, verification fields, numerical diagnostics and runtime. A zero exit code represents a verified optimal solve for this CLI contract; inspect the JSON when a solve stops at a resource or numerical limit. See the [status register](../project/STATUS.md) for the boundaries of each engine.

## Run the refinery qualification example

```sh
bash scripts/run-qualification-demo.sh
```

The script uses the checked-in **synthetic** `examples/refinery/refinery-feasible.mps` model. It builds the solver if needed, writes a result file under `/tmp`, prints the model path and shows the CLI verdict. This validates the software path for that example; it is not a plant approval. The [refinery case guide](../../examples/refinery/README.md) explains the model family and expected outcomes.

For the attributed historical public-input case:

```sh
./build/markov-cero-solve examples/refinery/fawley-public.mps
```

The public-input model uses approximate historical data and needs an engineer-owned shadow trial before operational use.

## Try other model classes

```sh
./build/markov-cero-solve examples/qp_portfolio.mps --engine qp
./build/markov-cero-solve examples/phase3/tiny_milp.mps --engine milp
./build/markov-cero-mps-inspect examples/refinery/refinery-feasible.mps
```

The example files are intentionally small and work offline. Large benchmark datasets are optional; use the [hash-pinned manifest](../../data/optional-datasets.json) and `python3 scripts/datasets.py --list` to see what can be restored. The default test suite does not require downloading a corpus.

## Check the build

```sh
ctest --test-dir build --output-on-failure
python3 scripts/check_docs.py
python3 scripts/check_source_limits.py
```

The full local release script also builds with two compilers and writes a report. It may take longer and requires both GCC and Clang; see [verification](VERIFY.md). For Python wheels, CUDA, benchmark comparison, CMake installation and troubleshooting, see [building](BUILDING.md).
