# Support, install, upgrade and rollback

Scope: roadmap backlog item 11, **code portion only**. This document records what
was actually executed on one host on 2026-09-28 (UTC timestamps in the
transcripts). The independent release review is **not** done: IR-33 and gate G7
stay open. No SBOM, no vulnerability review, no signed artifacts, no funded
support owner and no hosted sanitizer/CUDA evidence exist yet. Everything below
is local qualification, not a release claim.

Evidence: [packaging-qualification-20260928.json](../../evidence/packaging-qualification-20260928.json),
[packaging-verify-release-20260928.txt](../../evidence/packaging-verify-release-20260928.txt),
[rollback drill transcript](../../evidence/packaging-rollback-drill-20260928.txt),
[prefix drill transcript](../../evidence/packaging-prefix-drill-20260928.txt),
external consumer project: [evidence/packaging-consumer/](../../evidence/packaging-consumer/).

## 1. Supported platform matrix (D17)

| Tier | Configuration | Evidence / status |
|---|---|---|
| Supported locally | Linux x86-64 (glibc 2.44), gcc 16.2.1, CMake 4.x, `CMAKE_BUILD_TYPE=Release`, Python 3.14.7, CPU only | build + 89/89 CTest, wheel + 20 pytest, prefix install + external consumer — all on **this single host** |
| Preview only | clang 22.1.8 build of the same tree | recorded by the release-verification run; same host, no packaged artifact |
| Not supported | Windows, macOS, aarch64, any CUDA build, any prebuilt manylinux/musllinux wheel, Python < 3.14 (the wheel is `cp314`), any hosted CI configuration | never exercised here; must not be claimed |

The wheel produced locally carries the tag `linux_x86_64`, **not** `manylinux`.
It is a host wheel: installing it on another distribution or glibc version is
untested and unsupported. `requires-python = ">=3.9"` is declared metadata, not
a tested range — only 3.14.7 was exercised.

## 2. Support contact and response (D18 — honest placeholders)

- **Support contact:** *UNASSIGNED.* There is no funded support owner for this
  repository today. Raise issues in the repository that carries this document;
  nobody is obliged to answer.
- **Response SLA:** *NONE.* No response time, uptime or correctness guarantee is
  offered. An SLA requires funded staffing, which does not exist (D18).
- **Security contact:** *UNASSIGNED.* No disclosure channel has been
  established. Do not send vulnerability reports anywhere until a named security
  contact is published.
- **Named maintainers:** D18 requires two named maintainers plus a security
  contact **before public release**. Naming them is an external, human action
  and is tracked under IR-33; this document deliberately invents no names.

## 3. Install (Python wheel)

Build offline (no build isolation, no dependency downloads):

```sh
cd /home/saikrishna/markov-initial-build
.venv/bin/python -m pip wheel . -w /tmp/opencode/wheels-pkg --no-deps --no-build-isolation
# -> /tmp/opencode/wheels-pkg/markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl
```

Install into a fresh virtual environment and run the tests from **outside** the
repository so an in-tree binary cannot hide a packaging defect:

```sh
python3 -m venv /tmp/opencode/pkg11/venv-qual
/tmp/opencode/pkg11/venv-qual/bin/python -m pip install --no-index \
  --find-links /tmp/opencode/pkg11/pipdl pytest numpy
/tmp/opencode/pkg11/venv-qual/bin/python -m pip install --no-index --no-deps \
  /tmp/opencode/wheels-pkg/markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl
cd /tmp/opencode/pkg11/run
/tmp/opencode/pkg11/venv-qual/bin/python -m pytest \
  /home/saikrishna/markov-initial-build/python/tests -q
# -> 20 passed
```

`pytest` and `numpy` are **test-only** tools installed from a local wheel
directory (`--no-index`); neither is a runtime dependency of `markov-cero`
(`pyproject.toml` declares no runtime dependencies). Without `numpy` the same
run reports `15 passed, 5 skipped`.

Verify what was installed:

```sh
/tmp/opencode/pkg11/venv-qual/bin/python -m pip show markov-cero
sha256sum /tmp/opencode/wheels-pkg/markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl
```

## 4. Upgrade

```sh
/tmp/opencode/pkg11/venv-drill/bin/python -m pip install --no-index --no-deps \
  --force-reinstall /tmp/opencode/wheels-pkg/markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl
```

`--force-reinstall` is required for a same-version rebuild: the package version
is `0.5.2` for both the prior and the new build, so plain `pip install <wheel>`
is a no-op. Builds of an identical version are distinguished **only** by the
wheel SHA-256 and by the SHA-256 of the installed `_core*.so`.

## 5. Rollback (validated drill)

Recorded transcript:
[evidence/packaging-rollback-drill-20260928.txt](../../evidence/packaging-rollback-drill-20260928.txt).
Sequence and results:

| Step | Command (exact) | Result |
|---|---|---|
| D1 install prior build | `pip install --no-index --no-deps --force-reinstall /tmp/opencode/wheels/markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl` (sha256 `914136ac146af53e0a153c5a5ac010550f6f2ce05e586d40d3712b65822e941e`) | `_core*.so` sha256 `9700ebf3a0e0c5c830229dabd450080fd7006cc32ccfffde0708a8f016e5539b`; 20 passed |
| D2 upgrade | `pip install --no-index --no-deps --force-reinstall /tmp/opencode/wheels-pkg/markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl` (sha256 `fac615128c846e6b48c51252afb44fec199be30154e370f587309186a0145f4b`) | `_core*.so` sha256 `15fe24780b27a3db4c869cd88257bd10ea3f3dc24907e5d487df53de715324be`; 20 passed |
| D3 rollback | `pip install --no-index --no-deps --force-reinstall /tmp/opencode/wheels/...whl` (prior) | `_core*.so` back to `9700ebf3…e5539b` — **byte-identical to D1**; 20 passed |
| D4 uninstall | `pip uninstall -y markov-cero` | clean removal; `import markov_cero` raises `ModuleNotFoundError` |

Rollback command for an operator:

```sh
pip install --no-index --no-deps --force-reinstall <previously-accepted-wheel>
python -m pytest <your test suite>   # re-run your own acceptance tests
```

Keep the previously accepted wheel file and its SHA-256: because the version
string does not change between rebuilds, the checksum **is** the rollback
identity. Nothing is rolled back "by version" today.

## 6. Uninstall

```sh
<venv>/bin/python -m pip uninstall -y markov-cero
<venv>/bin/python -c "import markov_cero"   # expected: ModuleNotFoundError
```

## 7. C++ install prefix: install, reinstall, remove

Transcript: [evidence/packaging-prefix-drill-20260928.txt](../../evidence/packaging-prefix-drill-20260928.txt).

```sh
cmake -S . -B build_item11 -DCMAKE_BUILD_TYPE=Release
cmake --build build_item11 --parallel 4
cmake --install build_item11 --prefix /tmp/opencode/install-pkg     # P1
cmake -S evidence/packaging-consumer -B /tmp/opencode/pkg11/consumer-build \
  -DCMAKE_PREFIX_PATH=/tmp/opencode/install-pkg -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/opencode/pkg11/consumer-build --parallel 4
(cd /tmp/opencode/pkg11/consumer-build && ./installed-consumer \
  /home/saikrishna/markov-initial-build/examples/cases/multiperiod_production.mps)
# -> status=Optimal verified=yes certificate=independent_mip_tree objective=5300
#    replay accepted=yes checked_nodes=7 ... installed-consumer PASS
```

| Step | Command | Result |
|---|---|---|
| P1 install | `cmake --install build_item11 --prefix /tmp/opencode/install-pkg` | 83 files: `lib/libmarkov_cero_core.a`, 5 binaries, headers, `lib/cmake/markov_cero/*`, `share/markov-cero/{LICENSE,NOTICE,PROVENANCE.md}` |
| P2 remove | `rm -rf /tmp/opencode/install-pkg` then re-configure the consumer | configure **fails** (exit 1, "Could not find a package configuration file…") — expected |
| P3 reinstall | `cmake --install build_item11 --prefix /tmp/opencode/install-pkg` + rebuild consumer | consumer builds and passes again; installed `markov-cero-verify-mip MODEL proof.txt` prints `VERIFIED: …` (exit 0) |
| P4 remove | `rm -rf /tmp/opencode/install-pkg` | prefix gone |

Uninstall of the C++ package is `rm -rf <prefix>`; there is no generated
`install_manifest.txt` cleanup tool and none is claimed. The consumer project
is checked in under `evidence/packaging-consumer/` so the drill is reproducible.

## 8. What is NOT supported (read before filing an issue)

- **Untrusted input.** The MPS/NLP parsers enforce byte/line/row/column/nonzero
  limits (`io::MpsLimits`) and there are local fuzz targets, but there has been
  no independent security review and no adversarial input campaign. Treat input
  as **trusted**. Do not expose the CLI or parser to hostile files.
- **Hosted/CI evidence.** No hosted sanitizer, CUDA or multi-platform run backs
  this package. ASan/UBSan and TSan results in this repository come from local
  developer hosts only.
- **GPU / CUDA.** CUDA is a separate, unqualified support tier here: no CUDA
  build or device was exercised for this packaging work.
- **Portability beyond the matrix above** (§1): other OS/arch/distributions,
  other Python versions, other compilers — untested, therefore unsupported.
- **Correctness or performance guarantees.** Passing the wheel tests and the
  external consumer shows the package installs and solves one MILP with a
  replayed proof. It is not a correctness, performance or security claim.
- **Supply-chain assurances.** No SBOM, no signature, no pinned-dependency
  lockfile, no vulnerability scan, no reproducible-build comparison.

## 9. Release verification and manifest

```sh
./scripts/verify-release.sh     # check-sovereignty + gcc and clang Release builds + full ctest
python3 scripts/check_source_limits.py
python3 scripts/check_docs.py
```

By default `verify-release.sh` writes `evidence/local-verification-report.txt` and
`evidence/environment-local.json`. Both destinations are overridable without
editing the script:

```sh
MARKOV_CERO_VERIFY_REPORT=/tmp/my-report.txt \
MARKOV_CERO_VERIFY_ENVJSON=/tmp/my-environment.json ./scripts/verify-release.sh
```

For this qualification the report was redirected to the agent-owned
`evidence/packaging-verify-release-20260928.txt`, because
`evidence/local-verification-report.txt` is not owned by this work item. The
script now attempts **both** compilers even when the first one fails, and records
which configuration failed.

Release manifest (wheel identity + source manifest):

- Wheel: `markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl`,
  sha256 `fac615128c846e6b48c51252afb44fec199be30154e370f587309186a0145f4b`.
- Source: `evidence/readiness-validation/source-manifest.csv` (420 tracked paths
  with per-file sha256 and line counts), **not** regenerated by this work item —
  it describes an earlier snapshot, so it must be regenerated after the tree is
  committed before any release is cut.

## 10. Open release gates (IR-33 / G7)

All of the following remain **pending**; none was closed by this work item:

- IR-33: SBOM / vulnerability review — **pending**.
- IR-33: clean committed release (tree is an uncommitted worktree; manifests
  must be regenerated against a commit) — **pending**.
- IR-33: support ownership (two named maintainers + security contact, D18) —
  **pending**; §2 above stays a placeholder.
- IR-33: hosted sanitizer and CUDA evidence — **pending**.
- Independent mathematical/security/release review (roadmap §14 item 11,
  second half) — **pending**; commissioning it is an external action.
