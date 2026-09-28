from __future__ import annotations
from .dolan_more_profile_config import (
    DEFAULT_CSV, DEFAULT_INPUT, DEFAULT_SVG, argparse, os, sys
)
from .dolan_more_profile_generate_svg import generate_svg
from .dolan_more_profile_generate_svg import load_results
from .dolan_more_profile_generate_svg import log_grid
from .dolan_more_profile_generate_svg import performance_ratios
from .dolan_more_profile_generate_svg import rho_value
from .dolan_more_profile_generate_svg import write_profile_csv

def main() -> int:
    ap = argparse.ArgumentParser(description="Dolan-More performance profile (exact)")
    ap.add_argument("--input", default=DEFAULT_INPUT)
    ap.add_argument("--svg", default=DEFAULT_SVG)
    ap.add_argument("--csv", dest="csv_path", default=DEFAULT_CSV)
    ap.add_argument("--tau-max", type=float, default=100.0)
    ap.add_argument("--samples", type=int, default=101)
    args = ap.parse_args()

    if not os.path.isfile(args.input):
        print(f"[-] input not found: {args.input}", file=sys.stderr)
        return 2

    problems, solvers, success, runtime = load_results(args.input)
    if not problems or not solvers:
        print("[-] no rows in input", file=sys.stderr)
        return 1

    ratios = performance_ratios(problems, solvers, success, runtime)
    grid = log_grid(1.0, args.tau_max, args.samples)

    write_profile_csv(args.csv_path, problems, solvers, ratios, grid)
    generate_svg(args.svg, problems, solvers, ratios, grid, tau_max=args.tau_max)

    n_solved = sum(1 for p in problems if any(success.get((p, s)) for s in solvers))
    print(f"[+] problems |P| = {len(problems)} (solved by at least one solver: {n_solved})")
    print(f"[+] solvers = {', '.join(solvers)}")
    for s in solvers:
        solved = sum(1 for p in problems if success.get((p, s)))
        print(f"    {s:<12} solved {solved}/{len(problems)}  "
              f"rho(1)={rho_value(problems, ratios, s, 1.0):.3f}  "
              f"rho(10)={rho_value(problems, ratios, s, 10.0):.3f}  "
              f"rho({args.tau_max:g})={rho_value(problems, ratios, s, args.tau_max):.3f}")
    print(f"[+] profile svg -> {args.svg}")
    print(f"[+] profile data -> {args.csv_path}")
    return 0
