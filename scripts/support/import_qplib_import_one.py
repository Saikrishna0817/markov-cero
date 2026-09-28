from __future__ import annotations
from .import_qplib_config import (
    argparse, hashlib, json, pathlib, sys, zipfile
)
from .import_qplib_parse_annotated import parse_qplib
from .import_qplib_write_mps import solution_value
from .import_qplib_write_mps import write_mps

def import_one(source: pathlib.Path, out_dir: pathlib.Path, sol_dir: pathlib.Path | None = None) -> dict:
    if source.suffix == ".zip" or source.name.endswith(".qplib.zip"):
        with zipfile.ZipFile(source) as archive:
            members = [m for m in archive.namelist() if m.endswith(".qplib")]
            if not members:
                raise ValueError(f"{source}: no .qplib member")
            data = archive.read(members[0])
            inner_name = members[0]
    else:
        data = source.read_bytes()
        inner_name = source.name
    tmp = out_dir / "_tmp.qplib"
    tmp.write_bytes(data)
    try:
        instance = parse_qplib(tmp)
    finally:
        tmp.unlink()
    mps_path = out_dir / f"{instance['name']}.mps"
    write_mps(instance, mps_path)
    sol_path = sol_dir / f"{instance['name']}.sol" if sol_dir else None
    has_solution = sol_path is not None and sol_path.is_file()
    reference = solution_value(instance, sol_path) if has_solution else None
    provenance = {
        "source": str(source),
        "member": inner_name,
        "sha256": hashlib.sha256(data).hexdigest(),
        "name": instance["name"],
        "problem_class": instance["problem_class"],
        "direction": instance["direction"],
        "rows": instance["m"],
        "columns": instance["n"],
        "quadratic_nonzeros": len(instance["h"]),
        "linear_constraint_nonzeros": len(instance["a"]),
        "reference_objective": reference,
        "reference_solution": str(sol_path) if has_solution else None,
    }
    (out_dir / f"{instance['name']}.provenance.json").write_text(
        json.dumps(provenance, indent=2) + "\n"
    )
    return provenance

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", nargs="+", required=True, help=".qplib.zip files (or a directory)")
    parser.add_argument("--out", required=True, help="output directory for .mps + provenance")
    parser.add_argument(
        "--sols",
        default=None,
        help="directory of official QPLIB solution files (default: <out>/sol)",
    )
    args = parser.parse_args()

    inputs: list[pathlib.Path] = []
    for item in args.input:
        path = pathlib.Path(item)
        if path.is_dir():
            inputs.extend(sorted(path.glob("*.qplib.zip")))
            inputs.extend(sorted(path.glob("*.qplib")))
        else:
            inputs.append(path)
    if not inputs:
        print("no QPLIB sources found", file=sys.stderr)
        return 2

    out_dir = pathlib.Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    sol_dir = pathlib.Path(args.sols) if args.sols else out_dir / "sol"
    for source in inputs:
        try:
            info = import_one(source, out_dir, sol_dir)
        except Exception as exc:  # noqa: BLE001 - report and continue
            print(f"[-] {source}: {exc}", file=sys.stderr)
            continue
        ref = info["reference_objective"]
        marker = " [solution verified]" if info["reference_solution"] else ""
        print(
            f"[+] {info['name']}: {info['rows']}x{info['columns']} "
            f"({info['problem_class']}, {info['direction']}), "
            f"H-nnz={info['quadratic_nonzeros']}, "
            f"reference={('%.10g' % ref) if ref is not None else 'none'}{marker}"
        )
    return 0
