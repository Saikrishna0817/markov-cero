#!/usr/bin/env python3
"""markov-cero: End-to-End (E2E) Test Runner & Harness.

Clean-room sovereign Python script (zero third-party dependencies).
Executes E2E test suites, filters by Tier / Milestone / Feature,
and produces structured JSON and JUnit XML reports.
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from xml.etree import ElementTree as ET


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="markov-cero sovereign E2E test harness"
    )
    parser.add_argument(
        "--tier",
        type=str,
        default="all",
        help="Test tier filter: 1, 2, 3, 4, or all (default: all)",
    )
    parser.add_argument(
        "--milestone",
        type=str,
        default="all",
        help="Milestone filter: M1, M2, M3, M4, M5, M6, or all (default: all)",
    )
    parser.add_argument(
        "--feature",
        type=int,
        default=None,
        help="Feature ID filter: 1 to 38 (default: all)",
    )
    parser.add_argument(
        "--build-dir",
        type=str,
        default="build",
        help="Path to CMake build directory (default: build)",
    )
    parser.add_argument(
        "--json",
        dest="json_output",
        type=str,
        default=None,
        help="Path to write structured JSON test execution report",
    )
    parser.add_argument(
        "--xml",
        dest="xml_output",
        type=str,
        default=None,
        help="Path to write JUnit XML test execution report",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List all registered test cases in executables and exit",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose output during execution",
    )
    return parser.parse_args()


def find_test_binaries(build_dir: Path) -> list[Path]:
    candidates = [
        "e2e_tier1_m1_features",
        "e2e_tier2_m1_boundaries",
        "e2e_tier3_m1_combinations",
        "e2e_tier4_m1_scenarios",
    ]
    found = []
    for cand in candidates:
        for p in build_dir.rglob(cand):
            if p.is_file() and os.access(p, os.X_OK):
                found.append(p)
                break
    return sorted(found)


def query_test_list(binary: Path) -> list[dict]:
    cmd = [str(binary), "--list"]
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    tests = []
    for line in res.stdout.splitlines():
        line = line.strip()
        if line.startswith("[") and "]" in line:
            # Format: [T1_F01_01_NormalEqFactorizeSolve] Name [Tier: Tier1, Milestone: M1, Feature: 1]
            try:
                tid = line.split("]")[0][1:]
                rest = line.split("]", 1)[1].strip()
                name_part = rest.split("[Tier:")[0].strip()
                meta_part = "[Tier:" + rest.split("[Tier:")[1]
                tier = meta_part.split("Tier:")[1].split(",")[0].strip()
                milestone = meta_part.split("Milestone:")[1].split(",")[0].strip()
                feat_str = meta_part.split("Feature:")[1].split("]")[0].strip()
                tests.append(
                    {
                        "binary": str(binary),
                        "id": tid,
                        "name": name_part,
                        "tier": tier,
                        "milestone": milestone,
                        "feature_id": int(feat_str) if feat_str.isdigit() else 0,
                    }
                )
            except Exception:
                pass
    return tests


def run_binary(
    binary: Path,
    tier_filter: str,
    milestone_filter: str,
    verbose: bool,
) -> tuple[int, str, float]:
    cmd = [str(binary)]
    if tier_filter != "all":
        tier_arg = f"Tier{tier_filter}" if not tier_filter.startswith("Tier") else tier_filter
        cmd.extend(["--tier", tier_arg])
    if milestone_filter != "all":
        cmd.extend(["--milestone", milestone_filter])
    if verbose:
        cmd.append("--verbose")

    start_time = time.perf_counter()
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    elapsed = time.perf_counter() - start_time
    output = res.stdout + ("\n" + res.stderr if res.stderr else "")
    return res.returncode, output, elapsed


def parse_individual_results(output: str) -> list[dict]:
    results = []
    # Line format: RUN      T1_F01_01_NormalEqFactorizeSolve: Factorize and solve... ... OK (0.24 ms)
    # Line format: RUN      T1_F01_01_NormalEqFactorizeSolve: Factorize and solve... ... FAILED
    for line in output.splitlines():
        line = line.strip()
        if line.startswith("RUN "):
            parts = line[4:].split(" ... ")
            if len(parts) == 2:
                info_part = parts[0]
                status_part = parts[1]
                test_id = info_part.split(":")[0].strip()
                test_name = info_part.split(":", 1)[1].strip() if ":" in info_part else test_id
                passed = "OK" in status_part
                ms = 0.0
                if "(" in status_part and "ms)" in status_part:
                    try:
                        ms = float(status_part.split("(")[1].split("ms")[0].strip())
                    except ValueError:
                        pass
                results.append(
                    {
                        "id": test_id,
                        "name": test_name,
                        "passed": passed,
                        "status": "PASSED" if passed else "FAILED",
                        "time_ms": ms,
                    }
                )
    return results


def write_json_report(
    path: Path, summary: dict, test_details: list[dict]
) -> None:
    report = {
        "metadata": {
            "suite": "markov-cero-e2e",
            "version": "0.5.2",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        },
        "summary": summary,
        "tests": test_details,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)


def write_junit_xml(path: Path, test_details: list[dict]) -> None:
    testsuites = ET.Element("testsuites")
    suite = ET.SubElement(
        testsuites,
        "testsuite",
        name="markov_cero_e2e",
        tests=str(len(test_details)),
        failures=str(sum(1 for t in test_details if not t["passed"])),
        errors="0",
        time=str(sum(t.get("time_ms", 0.0) for t in test_details) / 1000.0),
    )
    for t in test_details:
        case = ET.SubElement(
            suite,
            "testcase",
            classname="markov_cero.e2e",
            name=t.get("id", "test"),
            time=str(t.get("time_ms", 0.0) / 1000.0),
        )
        if not t.get("passed", False):
            failure = ET.SubElement(
                case, "failure", message="Assertion failed"
            )
            failure.text = t.get("name", "Test failed")
    path.parent.mkdir(parents=True, exist_ok=True)
    tree = ET.ElementTree(testsuites)
    tree.write(path, encoding="utf-8", xml_declaration=True)


def main() -> int:
    args = parse_arguments()
    root_dir = Path(__file__).resolve().parent.parent
    build_dir = (root_dir / args.build_dir).resolve()

    if not build_dir.is_dir():
        print(f"Error: build directory {build_dir} not found. Run CMake build first.", file=sys.stderr)
        return 2

    binaries = find_test_binaries(build_dir)
    if not binaries:
        print(f"Error: no E2E test binaries found in {build_dir}. Compile E2E targets first.", file=sys.stderr)
        return 2

    if args.list:
        total_found = 0
        print("=== markov-cero E2E Test Inventory ===")
        for b in binaries:
            tests = query_test_list(b)
            total_found += len(tests)
            for t in tests:
                print(f"  [{t['tier']}] [{t['milestone']}] [F{t['feature_id']:02d}] {t['id']}: {t['name']}")
        print(f"Total Tests Discovered: {total_found}")
        return 0

    print("=" * 65)
    print(" markov-cero Sovereign E2E Test Runner")
    print(f" Filters: Tier={args.tier}, Milestone={args.milestone}, Feature={args.feature or 'all'}")
    print(f" Binaries: {len(binaries)} executables detected")
    print("=" * 65)

    all_test_details = []
    total_passed = 0
    total_failed = 0
    start_all = time.perf_counter()

    for binary in binaries:
        print(f"\n>> Executing suite: {binary.name}")
        ret, output, dur = run_binary(binary, args.tier, args.milestone, args.verbose)
        print(output.strip())
        parsed = parse_individual_results(output)
        for item in parsed:
            item["binary"] = binary.name
            if item["passed"]:
                total_passed += 1
            else:
                total_failed += 1
            all_test_details.append(item)

    total_time = time.perf_counter() - start_all
    summary = {
        "total_tests": total_passed + total_failed,
        "passed": total_passed,
        "failed": total_failed,
        "total_time_seconds": round(total_time, 3),
        "status": "PASSED" if total_failed == 0 and (total_passed > 0) else "FAILED",
    }

    print("\n" + "=" * 65)
    print(f" E2E RUN SUMMARY: {total_passed} PASSED, {total_failed} FAILED in {total_time:.3f}s")
    print(f" Overall Verdict: {summary['status']}")
    print("=" * 65)

    if args.json_output:
        json_path = Path(args.json_output).resolve()
        write_json_report(json_path, summary, all_test_details)
        print(f"Wrote JSON report to: {json_path}")

    if args.xml_output:
        xml_path = Path(args.xml_output).resolve()
        write_junit_xml(xml_path, all_test_details)
        print(f"Wrote JUnit XML report to: {xml_path}")

    return 0 if (total_failed == 0 and total_passed > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
