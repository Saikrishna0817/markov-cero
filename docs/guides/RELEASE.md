# Install, upgrade, rollback and support

Scope: blueprint **REL-01 — Package and deploy the supported solver**. This
document records what was executed on **one host** on 2026-10-01 (UTC timestamps
in the transcripts) and what the repository still does *not* provide. It is
local qualification, not a release claim: IR-33 and gate G7 stay open.

Evidence: [packaging-qualification-20261001.json](../../evidence/packaging-qualification-20261001.json),
[SBOM](../../evidence/release-sbom-20261001.json),
[source manifest](../../evidence/release-source-manifest-20261001.csv),
[dependency vulnerability scan](../../evidence/vulnerability-scan-20261001.json),
raw logs and drill transcripts in
[evidence/packaging-qualification-20261001/](../../evidence/packaging-qualification-20261001/),
external consumer project: [evidence/packaging-consumer/](../../evidence/packaging-consumer/).

## 1. Supported platform matrix (D17)

| Tier | Configuration | Evidence / status |
|---|---|---|
| Supported locally | Linux x86-64 (glibc 2.44), gcc 16.2.1, CMake 4.4.3, `CMAKE_BUILD_TYPE=Release` with `MARKOV_CERO_WARNINGS_AS_ERRORS=ON`, Python 3.14.7, CPU only | clean build + **120/120 CTest**, wheel + **27 pytest**, prefix install (93 files) + installed external consumer — all on **this single host**, commit `980687c` |
| Sanitizer-qualified locally | same tree with `MARKOV_CERO_ENABLE_ASAN_UBSAN=ON`, `RelWithDebInfo` | **120/120 CTest** (457 s); the sanitizer binary cannot run under the hosted `RLIMIT_AS`, see §8 |
| Preview only | clang build of the same tree | exercised by `scripts/verify-release.sh`; same host, no packaged artifact |
| Not supported | Windows, macOS, aarch64, any CUDA build, any prebuilt manylinux/musllinux wheel, Python other than 3.14 (the wheel is `cp314`), any hosted CI configuration | never exercised here; must not be claimed |

The wheel produced locally carries the tag `linux_x86_64`, **not** `manylinux`.
It is a host wheel: installing it on another distribution or glibc version is
untested and unsupported. `requires-python = ">=3.9"` is declared metadata, not
a tested range — only 3.14.7 was exercised.

`nvcc` is absent on this host, so **no CUDA compilation or device run backs this
package**. GPU and CUDA stay outside the supported tier.

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
  tracked under IR-33; this document deliberately invents no names.

## 3. Install (Python wheel)

Build offline (no build isolation, no dependency downloads):

```sh
cd /home/saikrishna/markov-initial-build
.venv/bin/python -m pip wheel . -w /tmp/opencode/rel01/wheels --no-deps --no-build-isolation
# -> markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl
```

Install into a fresh virtual environment and run the tests from **outside** the
repository so an in-tree binary cannot hide a packaging defect:

```sh
python3 -m venv /tmp/opencode/rel01/venv
/tmp/opencode/rel01/venv/bin/python -m pip install --no-index \
  --find-links /tmp/opencode/rel01/pipdl pytest numpy
/tmp/opencode/rel01/venv/bin/python -m pip install --no-index --no-deps \
  /tmp/opencode/rel01/wheels/markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl
mkdir -p /tmp/opencode/rel01/run && cd /tmp/opencode/rel01/run
/tmp/opencode/rel01/venv/bin/python -m pytest \
  /home/saikrishna/markov-initial-build/python/tests -q
# -> 27 passed
```

`pytest` and `numpy` are **test-only** tools installed from a local wheel
directory (`--no-index`); neither is a runtime dependency of `markov-cero`
(`pyproject.toml` declares no runtime dependencies).

Verify what was installed:

```sh
/tmp/opencode/rel01/venv/bin/python -m pip show markov-cero
sha256sum /tmp/opencode/rel01/wheels/*.whl
# -> c1f19c9d3dc809f143e5a8b8956823fbb4aff3befab4f3a99479fe7344f79d91
```

## 4. Upgrade

```sh
<venv>/bin/python -m pip install --no-index --no-deps --force-reinstall \
  /path/to/markov_cero-0.5.2-*.whl
```

`--force-reinstall` is required for a same-version rebuild: the package version
is `0.5.2` for both the prior and the new build, so plain `pip install <wheel>`
is a no-op. Builds of an identical version are distinguished **only** by the
wheel SHA-256 and by the SHA-256 of the installed `_core*.so`.

## 5. Rollback (validated drill)

Transcript:
[rollback-drill-20261001.txt](../../evidence/packaging-qualification-20261001/rollback-drill-20261001.txt).
Prior wheel = MINLP-01 commit `2fa49ea` (sha256 `5a55f57d…57ebd9`); new wheel =
commit `980687c` (sha256 `c1f19c9d…79d91`).

| Step | Command (exact) | Result |
|---|---|---|
| D1 install prior | `pip install --no-index --no-deps --force-reinstall <prior wheel>` | `_core*.so` sha256 `b1a82348…8a2c265`; **26 passed, 1 failed** |
| D2 upgrade | same command with the new wheel | `_core*.so` sha256 `14dc88a8…8b01a16`; **27 passed** |
| D3 rollback | same command with the prior wheel | `_core*.so` back to `b1a82348…8a2c265` — **byte-identical to D1**; 26 passed, 1 failed |
| D4 uninstall | `pip uninstall -y markov-cero` | clean removal; `import markov_cero` raises `ModuleNotFoundError` |
| D1b control | prior wheel against **its own commit's** test suite | **26 passed** |

The one failure on the prior wheel is `test_proof_guarantee.py::test_minlp_oa_proof_guarantee_fields`,
a MINLP-02 field that did not exist at MINLP-01. It is version skew between an
old wheel and a newer test file, not a packaging defect; D1b shows the prior
build passes its own suite.

Operator rollback:

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

Transcript:
[prefix-drill-20261001.txt](../../evidence/packaging-qualification-20261001/prefix-drill-20261001.txt).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMARKOV_CERO_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel 12
cmake --install build --prefix /tmp/opencode/rel01/install          # P1
cmake -S evidence/packaging-consumer -B /tmp/opencode/rel01/consumer-build \
  -DCMAKE_PREFIX_PATH=/tmp/opencode/rel01/install -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/opencode/rel01/consumer-build --parallel 4
(cd /tmp/opencode/rel01/consumer-build && ./installed-consumer \
  /home/saikrishna/markov-initial-build/examples/cases/multiperiod_production.mps)
# -> status=Optimal verified=yes certificate=independent_mip_tree objective=5300
#    fingerprint=3743977559983760680 replay accepted=yes checked_nodes=7
#    installed-consumer PASS
```

| Step | Command | Result |
|---|---|---|
| P1 install | `cmake --install build --prefix …` | **93 files**: static core, 6 binaries, headers, `lib/cmake/markov_cero/*`, `share/markov-cero/*`; every `bin/` and `lib/` file byte-identical to the build tree |
| P2 remove | `rm -rf <prefix>` then re-configure the consumer | configure **fails** (exit 1) — expected |
| P3 reinstall | `cmake --install` + rebuild consumer | consumer builds and passes again |
| P4 remove | `rm -rf <prefix>` | prefix gone |

Uninstall of the C++ package is `rm -rf <prefix>`; there is no generated
`install_manifest.txt` cleanup tool and none is claimed. The consumer project is
checked in under `evidence/packaging-consumer/` so the drill is reproducible.

Full per-file digests:
[evidence/packaging-qualification-20261001/binary-hashes.json](../../evidence/packaging-qualification-20261001/binary-hashes.json).

## 8. Hosted service (optional, bounded)

The HTTP adapter is documented by the contract
[hosted-limits.md](../contracts/hosted-limits.md) (**v1.1**), not by this guide.
The service is *optional*: the CLI and library need no server.

- **Process caps** (`RLIMIT_CPU`, `RLIMIT_AS`, `RLIMIT_FSIZE`, `RLIMIT_NOFILE`,
  `RLIMIT_NPROC`, wall-clock timeout, output-size cap) are enforced on the solver
  child only; checked by CTest `hosted_os_limits`.
- **Request quota**: a fixed 60 s window keyed by client address, checked
  **before** authentication, answered as HTTP 429; exercised by CTest
  `hosted_service_limits`.
- **Metrics**: `GET /metrics` returns monotonically increasing counters
  (`requests_total`, `responses_by_status{...}`, `quotas_rejected_total`,
  `solve_seconds_total`, `workers_killed_total`, …). Counters are snapshots
  taken before their own response is counted.
- **Result fields**: `assurance`, `lp_iterations`, `relative_gap`,
  `stop_reason` are whitelisted; unknown payload fields never reach a client.
- **Sanitizer builds cannot be deployed**: an ASan child reserves ~14 TB of
  shadow address space and is killed by the declared 1 GiB `RLIMIT_AS`. The
  hosted tests lift only that bound when pointed at a sanitizer build and print
  that they did so. Deploying a sanitizer build is not a supported
  configuration, and the adapter itself lifts nothing.

Local run of the two hosted tests:

```sh
ctest --test-dir build -R hosted_ --output-on-failure   # 2/2 passed
```

Deployment shape (Docker `--memory`, cgroups, reverse proxy, TLS) is the
deployer's responsibility and has **not** been exercised here.

## 9. SBOM and release records

```sh
.venv/bin/python scripts/generate_release_sbom.py \
  --wheel /tmp/opencode/rel01/wheels/markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl \
  --artifact-hashes evidence/packaging-qualification-20261001/binary-hashes.json \
  --out evidence/release-sbom-20261001.json
```

Writes an SPDX 2.3 document plus
`evidence/release-source-manifest-20261001.csv` (one sha256 per tracked file,
regenerated against this commit — an older readiness manifest must never be
reused). Offline and deterministic apart from the timestamp.

The document records its own limitations, verbatim: "no package signature or
provenance attestation", "no vulnerability scan or advisory database lookup",
"no reproducible-build comparison against a second builder", "no lockfile and
no dependency hash pinning", single host and single toolchain, `third_party/`
holds licenses only. The scan record was produced *after* that document, so
the SBOM neither embeds nor substitutes for it; the generator now phrases
those two lines to point at the records below whenever they exist, and
`evidence/release-sbom-20261001.json` keeps its original wording because a
generated record is never rewritten.

### 9.1 Dependency pinning

Pinned, and what each pin buys:

| What | Pin | Where |
|---|---|---|
| Qualification environment | 34 exact `==` versions from `pip freeze` | [`requirements/qualification.txt`](../../requirements/qualification.txt) |
| Build backend (bounded) | `setuptools>=77,<85`, `pybind11>=2.12,<4`, `cmake>=3.25,<5` | [`pyproject.toml`](../../pyproject.toml) `[build-system]` |
| CI actions | full commit SHAs (`checkout` `11d5960…`, `setup-python` `a26af69…`, `setup-node` `49933ea…`) | [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) |
| CI container | tag **and** digest `nvidia/cuda:12.6.3-devel-ubuntu24.04@sha256:392c0df7…` | same workflow |
| CI Python packages | `pip==26.2.1`, `pytest==9.1.1`, `numpy==2.5.3` | same workflow |

The qualified versions behind the bounded build requirements are setuptools
**84.0.0**, pybind11 **3.1.0** and cmake **4.4.3** (also in the freeze file).
Exact `==` pins were deliberately *not* written into `[build-system]`:
setuptools 84 requires Python >=3.10 while this project declares
`requires-python = ">=3.9"`, so an exact pin would make the wheel
unbuildable on 3.9. The bounds stop a silent major bump; the freeze file
records what was actually run.

Provenance of the pins, checked 2026-10-01: each action SHA was read back
from upstream with `git ls-remote … refs/tags/v4`/`v5` and equals the tag's
current target, and the container digest is the one Docker Hub reports for
that tag. CI's interpreter is pinned to `3.14.7`, the version the
qualification ran — required, not cosmetic: `numpy==2.5.3` needs Python
>=3.12, so the pins cannot install on the 3.11 CI used until now.

Not pinned, stated rather than implied: apt packages resolved inside the
container at run time, the `ubuntu-latest` runner label, the host toolchain
(g++ 16.2.1 / CMake 4.4.3 are recorded per record, not enforced), and the
`ml-training` floor (`torch>=2.9`). No file here carries hashes — this is
version pinning, not `--require-hashes` supply-chain verification.

### 9.2 Known-vulnerability scan (2026-10-01)

[Record](../../evidence/vulnerability-scan-20261001.json): `pip-audit`
2.10.1, run from an isolated venv outside the repository, against
`requirements/qualification.txt` on both the **PyPI** and **OSV** advisory
services (two sources, same answer).

- As committed, **32 of the 34 pins** are resolvable against PyPI and have
  **0 known vulnerabilities**; a separate pass over the installed `.venv`
  (56 distributions after excluding the audit tool's own packages) agrees.
- The two that are not resolvable are `markov-cero` (local wheel, not
  published) and `torch==2.9.1+cpu` (a local version PyPI does not carry) —
  both are silently *skipped* by a naive audit, which is how a clean-looking
  run can hide the one package that matters. The record therefore re-audits
  torch at its PyPI release version `2.9.1`, where it has **4 known
  vulnerabilities**
  (`PYSEC-2025-194`, `PYSEC-2025-195`, `PYSEC-2026-139`, `PYSEC-2026-2286`;
  fixes at 2.10.0/2.13.0 for three of them, **no upstream fix** for
  CVE-2026-4538).
- All four sit in the optional `ml-training` extra, which the wheel never
  imports — the wheel declares no runtime dependencies at all. They are
  recorded as open and accepted in [`pyproject.toml`](../../pyproject.toml),
  not silently fixed by bumping a floor that has never been qualified.

Not claimed by the scan: the npm tree under `web/` (lockfile only, no audit
run), C++ / system / apt packages, the container image or GPU driver stack,
`third_party/` beyond the licenses it stores, any source-level review, and
anything at all about tomorrow — it is a point-in-time observation and must
be rerun before a release.

## 10. What is NOT supported (read before filing an issue)

- **Untrusted input.** The MPS/NLP parsers enforce byte/line/row/column/nonzero
  limits (`io::MpsLimits`) and there are local fuzz targets, but there has been
  no independent security review and no adversarial input campaign. Treat input
  as **trusted**. Do not expose the CLI or parser to hostile files.
- **Hosted/CI evidence.** `.github/workflows/ci.yml` declares gcc/clang ×
  Debug/Release/ASan-UBSan/TSan/CUDA-compile/wheel/docs/web jobs, but **no hosted
  run is recorded in this repository**. All sanitizer results here come from a
  local developer host.
- **GPU / CUDA.** No CUDA build or device was exercised for this packaging work;
  `nvcc` is absent on the qualifying host.
- **Portability beyond §1**: other OS/arch/distributions, other Python versions,
  other compilers — untested, therefore unsupported.
- **Correctness or performance guarantees.** Passing the wheel tests and the
  external consumer shows the package installs and solves one MILP with a
  replayed proof. It is not a correctness, performance or security claim. For
  measured performance read [BENCHMARKS.md](BENCHMARKS.md).
- **Supply-chain assurances.** No signature, no pinned-dependency lockfile, no
  vulnerability scan, no reproducible-build comparison.
- **Production ML, NLP or global-MINLP tiers.** This package must not be
  advertised as production support for refinery, GPU/ML/NLP/MINLP workloads.

## 11. Release verification

```sh
./scripts/verify-release.sh                       # sovereignty + gcc/clang builds + full ctest
python3 scripts/check_source_limits.py
python3 scripts/check_docs.py
.venv/bin/python scripts/check_json.py
python3 scripts/link_backlinks.py --check
```

`verify-release.sh` writes `evidence/local-verification-report.txt` and
`evidence/environment-local.json` by default; both destinations are overridable
with `MARKOV_CERO_VERIFY_REPORT` and `MARKOV_CERO_VERIFY_ENVJSON`. The script
attempts **both** compilers even when the first one fails and records which
configuration failed.

For this qualification the report was redirected away from
`evidence/local-verification-report.txt`, which this work item does not own.

## 12. Open release gates (IR-33 / G7)

REL-01 left all of the following pending. Three have since been closed **on
this host only** (reproducible builds, dependency scan, dependency pinning);
the rest remain **pending**:

- IR-33: independent release/security review — **pending**; commissioning it is
  an external action.
- IR-33: support ownership (two named maintainers + security contact, D18) —
  **pending**; §2 stays a placeholder.
- IR-33: hosted sanitizer and CUDA evidence — **pending**; the workflow exists,
  no run record does.
- Second-host install and reproduction by an independent user — **pending**;
  everything above ran on one host.
- Signed artifacts — **pending**; nothing in this repository is signed.
- Vulnerability scan — **done locally, scope-limited**: Python packages only,
  point-in-time, two advisory sources agreeing, four known issues left open
  in the optional `ml-training` extra (§9.2,
  [record](../../evidence/vulnerability-scan-20261001.json)). No npm, C++,
  system, container or driver scan.
- Dependency pinning — **done locally, scope-limited**: environment freeze,
  bounded build requirements and SHA/digest-pinned CI (§9.1); no hashes, and
  the build-toolchain, apt and runner inputs stay unpinned.
- Reproducible-build comparison — **done locally, scope-limited**: two
  independently configured clean Release + `-Werror` build trees of one
  commit, at different absolute paths, produced **107/107 byte-identical**
  artifacts (0 differing, 0 tree-specific), and both trees passed
  120/120 CTest ([record](../../evidence/reproducible-build-20261001.json)).
  That closes the comparison itself only — it says nothing about a second
  host, another toolchain, the wheel or prefix packages, or any commit
  other than the one recorded.
