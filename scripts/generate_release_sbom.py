#!/usr/bin/env python3
"""Blueprint REL-01 step 4: SBOM plus source and binary hashes.

Writes two artifacts:
  * a CSV manifest with one sha256 per tracked source file, regenerated
    against the commit being released (the older readiness manifest
    describes an earlier snapshot and must not be reused);
  * an SPDX 2.3 JSON document describing the released package, the build
    and test dependencies that were actually resolved, and the built
    artifacts with their sha256 checksums.

Offline and deterministic apart from the timestamp: no network, no
vulnerability scan, no signing. Those absences are recorded in the
document's limitations rather than implied.

Usage:
  python3 scripts/generate_release_sbom.py --wheel W.whl \
      --artifact-hashes hashes.json --out evidence/release-sbom-DATE.json
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPDX_TO_ALGORITHM = {"sha256": "SHA256"}


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def tracked_files():
    names = git("ls-files", "-z").split("\0")
    for name in sorted(n for n in names if n):
        path = ROOT / name
        if path.is_file():
            yield name, path


def write_manifest(destination, generated=()):
    """One sha256 per tracked file, minus the generated outputs themselves.

    The manifest and the SBOM document cannot carry their own hashes —
    writing one changes the other — so both are excluded and the exclusion
    is stated in the document rather than left as a silent mismatch.
    """
    skip = {path.resolve() for path in generated}
    rows = []
    for name, path in tracked_files():
        if path.resolve() in skip:
            continue
        data = path.read_bytes()
        rows.append((name, hashlib.sha256(data).hexdigest(), len(data)))
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["path", "sha256", "bytes"])
        writer.writerows(rows)
    return rows


def package(identifier, name, version, filename=None, checksum=None):
    entry = {
        "SPDXID": f"SPDXRef-{identifier}",
        "name": name,
        "versionInfo": version,
        "downloadLocation": "NOASSERTION",
        "filesAnalyzed": False,
        "licenseConcluded": "Apache-2.0",
        "licenseDeclared": "Apache-2.0",
        "copyrightText": "NOASSERTION",
        "supplier": "NOASSERTION",
    }
    if filename:
        entry["packageFileName"] = filename
    if checksum:
        entry["checksums"] = [{"algorithm": SPDX_TO_ALGORITHM["sha256"],
                               "checksumValue": checksum}]
    return entry


def under_root(raw):
    """Resolve a CLI path; relative paths are interpreted from the repo root."""
    path = pathlib.Path(raw)
    return (path if path.is_absolute() else ROOT / path).resolve()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wheel", action="append", default=[],
                        help="built wheel to record (repeatable)")
    parser.add_argument("--artifact-hashes",
                        help="JSON file mapping installed paths to sha256")
    parser.add_argument("--manifest-destination",
                        help="CSV destination; defaults next to --out")
    parser.add_argument("--out", required=True, help="SPDX JSON destination")
    args = parser.parse_args(argv)

    out = under_root(args.out)
    manifest_path = (under_root(args.manifest_destination)
                     if args.manifest_destination
                     else out.with_name("release-source-manifest-"
                                        + out.stem.split("-")[-1] + ".csv"))
    rows = write_manifest(manifest_path, generated=(out, manifest_path))

    commit = git("rev-parse", "HEAD")
    staged = [line for line in git("diff", "--cached", "--name-only").splitlines()
              if line]
    unstaged = [line for line in git("diff", "--name-only").splitlines() if line]
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    created = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()

    packages = [package("Package-markov-cero", "markov-cero", version,
                        filename=f"markov-cero-{version} source tree")]
    resolved = [
        ("setuptools", "84.0.0", "build backend (pyproject build-system)"),
        ("pybind11", "3.1.0", "build backend (pyproject build-system)"),
        ("pytest", "9.1.1", "test-only; not a runtime dependency"),
        ("numpy", "2.5.3", "test-only; not a runtime dependency"),
    ]
    for name, resolved_version, role in resolved:
        entry = package(f"Package-dep-{name}", name, resolved_version)
        entry["comment"] = role
        packages.append(entry)
    for name, constraint in (("torch", ">=2.9"), ("onnx", ">=1.16"),
                             ("onnxruntime", ">=1.19")):
        entry = package(f"Package-optional-{name}", name, constraint)
        entry["comment"] = ("optional ml-training extra; not installed and "
                            "not exercised by this build")
        entry["downloadLocation"] = "NOASSERTION"
        packages.append(entry)

    artifacts = {}
    for wheel in args.wheel:
        path = under_root(wheel)
        if not path.is_file():
            raise SystemExit(f"wheel not found: {path}")
        entry = package(f"Artifact-wheel-{path.stem}", path.name, version,
                        filename=path.name, checksum=sha256_file(path))
        entry["comment"] = "built locally; tag is a host wheel, not manylinux"
        packages.append(entry)
        artifacts[path.name] = sha256_file(path)
    if args.artifact_hashes:
        installed = json.loads(under_root(args.artifact_hashes).read_text())
        for rel, digest in sorted(installed.items()):
            if not rel.startswith(("bin/", "lib/")):
                continue
            entry = package(f"Artifact-{rel.replace('/', '-').replace('.', '-')}",
                            pathlib.Path(rel).name, version,
                            filename=rel, checksum=digest)
            entry["comment"] = "installed from this build; identical to the build-tree file"
            packages.append(entry)
            artifacts[rel] = digest

    relationships = [
        {"spdxElementId": "SPDXRef-DOCUMENT",
         "relatedSpdxElement": "SPDXRef-Package-markov-cero",
         "relationshipType": "DESCRIBES"},
    ]
    for entry in packages[1:]:
        relationships.append({
            "spdxElementId": "SPDXRef-Package-markov-cero",
            "relatedSpdxElement": entry["SPDXID"],
            "relationshipType": ("DEPENDS_ON" if entry["SPDXID"].startswith(
                "SPDXRef-Package-dep") else "OTHER"),
        })

    document = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"markov-cero-{version}",
        "documentNamespace": f"urn:spdx-document:markov-cero:{version}:{commit}",
        "creationInfo": {
            "created": created,
            "creators": ["Tool: scripts/generate_release_sbom.py"],
        },
        "packages": packages,
        "relationships": relationships,
        "x_markovNotes": {
            "commit": commit,
            "generated_from": (
                "the git index: every listed path is either identical to "
                f"{commit[:7]} or staged for the release commit that records "
                "this document. unstaged modifications at generation: "
                f"{len(unstaged)}; staged paths: {len(staged)}"),
            "staged_paths": staged,
            "license": "Apache-2.0",
            "license_files": {"LICENSE": sha256_file(ROOT / "LICENSE"),
                              "NOTICE": sha256_file(ROOT / "NOTICE")},
            "source_manifest": {
                "path": str(manifest_path.relative_to(ROOT)),
                "entries": len(rows),
                "sha256": sha256_file(manifest_path),
                "covers": ("every git-tracked file at this commit except "
                           "this manifest and the SBOM document, which are "
                           "generated outputs and cannot hash themselves; "
                           "their hashes are the ones git records"),
            },
            "artifacts": artifacts,
                "limitations": [
                    "no package signature or provenance attestation",
                    "no vulnerability scan or advisory database lookup",
                    "no reproducible-build comparison against a second builder",
                    "dependencies are the versions resolved on this single host; "
                    "there is no lockfile and no dependency hash pinning",
                    "third_party/ holds licenses only, no vendored code",
                    "single host, single toolchain: no cross-distro or cross-arch "
                    "wheel was produced or tested",
                    "the source manifest excludes itself and this document, "
                    "because writing one changes the other's hash",
                ],
        },
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(document, indent=1, sort_keys=False) + "\n",
                   encoding="utf-8")
    print(f"wrote {out} ({len(packages)} packages, {len(rows)} source files)")
    print(f"wrote {manifest_path} ({len(rows)} entries)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
