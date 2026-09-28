#!/usr/bin/env python3
"""
markov-cero Crossover Study Plot Generator (T-5.13 / Gap 8 feature 13)
Generates high-resolution standalone SVG visualization and terminal ASCII
plots comparing CPU Simplex, CPU PDLP, and GPU PDLP wall-clock scaling.
Zero external dependencies (pure Python standard library).

Input: evidence/benchmarks/crossover_study.csv with the fixed schema
    instance,backend,nnz,status,verified,total_ms
where backend is one of cpu_simplex / cpu_pdlp / gpu_pdlp (legacy rows that
only say cpu/gpu are mapped onto the PDLP series they were produced with, and
legacy wide CSVs such as reports/crossover_study.csv are still accepted).

Semantics preserved from the original plot: log-log wall-clock time vs problem
size, three series (red simplex / green dashed CPU PDLP / blue GPU PDLP),
orange dashed empirical crossover marker N*. Only runs whose solver-reported
status is Optimal are plotted; TimeLimit / IterationLimit / ResourceLimit runs
are reported as text (never plotted as if they were solve times).
"""

import argparse
import csv
import math
import os
import sys
from typing import List, Dict, Any, Optional

SOLVED_STATUS = "Optimal"

# Plot series in draw order; backend tokens in the long CSV map onto them.
SERIES_ORDER = ["simplex", "cpu_pdlp", "gpu"]
BACKEND_TO_SERIES = {
    "cpu_simplex": "simplex",
    "cpu_pdlp": "cpu_pdlp",
    "gpu_pdlp": "gpu",
    # legacy rows from the pre-scale-up study (--engine pdlp for both rows)
    "cpu": "cpu_pdlp",
    "gpu": "gpu",
}
SERIES_STYLE = {
    "simplex": {"label": "CPU Simplex (O(m^2.5) pivoting)",
                "color": "#ef4444", "dash": "", "radius": 4.0},
    "cpu_pdlp": {"label": "CPU PDLP (single-thread first-order)",
                 "color": "#10b981", "dash": "4,2", "radius": 3.5},
    "gpu": {"label": "GPU PDLP (CUDA SpMV + H2D/D2H)",
            "color": "#38bdf8", "dash": "", "radius": 4.5},
}


def _to_float(val: Any) -> float:
    try:
        return float(val)
    except (ValueError, TypeError):
        return float("nan")


def _to_int(val: Any) -> int:
    try:
        return int(float(val))
    except (ValueError, TypeError):
        return 0


def _is_solved(entry: Dict[str, Any]) -> bool:
    status = (entry.get("status") or "").strip()
    if status and status != SOLVED_STATUS:
        return False
    ms = entry.get("ms", float("nan"))
    return ms == ms and ms > 0.0


def _solved_ms(entry: Optional[Dict[str, Any]]) -> float:
    if entry and _is_solved(entry):
        return entry["ms"]
    return float("nan")


def parse_crossover_csv(filepath: str) -> List[Dict[str, Any]]:
    records: List[Dict[str, Any]] = []
    index: Dict[str, Dict[str, Any]] = {}
    with open(filepath, "r", newline="") as f:
        reader = csv.DictReader(f)
        fields = reader.fieldnames or []
        wide = "backend" not in fields
        for row in reader:
            if wide:
                records.append({
                    "instance": row.get("instance", ""),
                    "nonzeros": _to_int(row.get("nonzeros", row.get("nnz", "0"))),
                    "series": {
                        "simplex": {
                            "status": row.get("simplex_status", ""),
                            "ms": _to_float(row.get("simplex_time_ms")),
                            "verified": row.get("pass", "True").lower() == "true",
                        },
                        "cpu_pdlp": {
                            "status": row.get("cpu_pdlp_status", ""),
                            "ms": _to_float(row.get("cpu_pdlp_time_ms")),
                            "verified": row.get("pass", "True").lower() == "true",
                        },
                        "gpu": {
                            "status": row.get("gpu_pdlp_status", ""),
                            "ms": _to_float(row.get("gpu_total_ms")),
                            "verified": row.get("gpu_verified", "True").lower() == "true",
                        },
                    },
                })
                continue

            name = row.get("instance", "")
            rec = index.get(name)
            if rec is None:
                rec = {"instance": name,
                       "nonzeros": _to_int(row.get("nnz", "0")),
                       "series": {}}
                index[name] = rec
                records.append(rec)
            key = BACKEND_TO_SERIES.get((row.get("backend") or "").strip())
            if key is None:
                continue
            rec["series"][key] = {
                "status": (row.get("status") or "").strip(),
                "ms": _to_float(row.get("total_ms")),
                "verified": (row.get("verified") or "").strip().lower() == "true",
            }
    records.sort(key=lambda r: (r["nonzeros"], r["instance"]))
    return records


def _format_ms(entry: Optional[Dict[str, Any]], width: int) -> str:
    if entry is None:
        return "N/A".rjust(width)
    if _is_solved(entry):
        return ("%.*f" % (3 if entry["ms"] < 100 else 1, entry["ms"])).rjust(width)
    status = (entry.get("status") or "?").strip()
    return status[:width].rjust(width)


def _flag(entry: Optional[Dict[str, Any]]) -> str:
    """Compact verification flag: V verified / u unverified / . not solved / - absent."""
    if entry is None:
        return "-"
    if _is_solved(entry):
        return "V" if entry.get("verified") else "u"
    return "."


def find_crossover(records: List[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
    """First instance (ascending nnz) where a first-order engine beats simplex."""
    for rec in records:
        s_ms = _solved_ms(rec["series"].get("simplex"))
        if s_ms != s_ms:
            continue
        options = [(key, _solved_ms(rec["series"].get(key)))
                   for key in ("cpu_pdlp", "gpu")]
        options = [(key, ms) for key, ms in options if ms == ms]
        if not options:
            continue
        key, o_ms = min(options, key=lambda kv: kv[1])
        if o_ms < s_ms:
            return {"instance": rec["instance"], "nonzeros": rec["nonzeros"],
                    "series": key}
    return None


def print_ascii_summary(records: List[Dict[str, Any]]):
    print("\n" + "=" * 100)
    print(" markov-cero Scale Crossover Study — Empirical Scaling Summary (Gap 8 / T-5.13)")
    print("=" * 100)
    header = ("{:<12} {:>9} | {:>13} | {:>13} | {:>13} | {:>10} | {:>9}")
    print(header.format("Instance", "NNZ", "Simplex(ms)", "CPUp(ms)", "GPUp(ms)",
                        "SPD(S/FO)", "Verified"))
    print("-" * 100)

    present = [k for k in SERIES_ORDER
               if any(k in rec["series"] for rec in records)]
    attempted_unsolved = []

    for rec in records:
        s = rec["series"].get("simplex")
        c = rec["series"].get("cpu_pdlp")
        g = rec["series"].get("gpu")
        s_ms = _solved_ms(s)
        c_ms = _solved_ms(c)
        g_ms = _solved_ms(g)
        fo_ms = g_ms if g_ms == g_ms else c_ms
        if s_ms == s_ms and fo_ms == fo_ms and fo_ms > 0:
            spd = "%9.1fx" % (s_ms / fo_ms)
        else:
            spd = "        -"
        flags = ["%s:%s" % (key[0], _flag(rec["series"].get(key)))
                 for key in present]
        print("{:<12} {:>9} | {} | {} | {} | {} | {:>9}".format(
            rec["instance"], rec["nonzeros"],
            _format_ms(s, 13), _format_ms(c, 13), _format_ms(g, 13),
            spd, " ".join(flags)))
        for key in present:
            entry = rec["series"].get(key)
            if entry is not None and not _is_solved(entry):
                attempted_unsolved.append((rec["instance"], key,
                                           entry.get("status") or "?"))

    print("  Verified flags: V=Optimal+verified, u=Optimal not verified, "
          ".=run did not reach Optimal, -=no such run")
    print("-" * 100)
    crossover = find_crossover(records)
    if crossover:
        if crossover["nonzeros"] <= records[0]["nonzeros"]:
            print("[*] Empirical Crossover Point N* <= smallest measured instance "
                  "(%s, %s nonzeros):" % (crossover["instance"],
                                          format(crossover["nonzeros"], ",")))
            print("    %s beats CPU simplex already at the smallest scale measured,"
                  % crossover["series"].replace("_", " "))
            print("    so N* is at or below %s nonzeros; the whole measured range"
                  % format(crossover["nonzeros"], ","))
            print("    sits in the first-order regime (nothing smaller was run).")
        else:
            print("[*] Empirical Crossover Point N*: %s (~%s nonzeros, %s wins)"
                  % (crossover["instance"], format(crossover["nonzeros"], ","),
                     crossover["series"].replace("_", " ")))
            print("    Below N*: simplex pivoting is cheaper; above N*: first-order")
            print("    PDLP SpMV amortizes and takes the lead.")
    else:
        simplex_comparisons = sum(
            _solved_ms(rec["series"].get("simplex")) == _solved_ms(rec["series"].get("simplex"))
            and any(_solved_ms(rec["series"].get(k)) == _solved_ms(rec["series"].get(k))
                    for k in ("cpu_pdlp", "gpu"))
            for rec in records
        )
        if simplex_comparisons == 0:
            print("[*] No simplex-to-first-order crossover can be estimated: "
                  "no comparable CPU simplex result is present.")
        else:
            print("[*] No first-order engine beats simplex in the comparable runs.")
        cpu_gpu = [(_solved_ms(rec["series"].get("cpu_pdlp")),
                    _solved_ms(rec["series"].get("gpu"))) for rec in records]
        cpu_gpu = [(cpu, gpu) for cpu, gpu in cpu_gpu if cpu == cpu and gpu == gpu]
        if cpu_gpu and all(cpu < gpu for cpu, gpu in cpu_gpu):
            print("[*] CPU PDLP is faster than GPU PDLP in all %d comparable rows."
                  % len(cpu_gpu))
    if attempted_unsolved:
        print("[!] runs that never reached Optimal (kept honest in the CSV):")
        for inst, key, status in attempted_unsolved:
            print("      %-12s %-11s %s" % (inst, key, status))
    print("=" * 100 + "\n")


def generate_svg(records: List[Dict[str, Any]],
                 output_path: str,
                 crossover_nnz: Optional[int] = None):
    valid_records = [r for r in records if r["nonzeros"] > 0]
    if not valid_records:
        print("no usable records; skipping %s" % output_path, file=sys.stderr)
        return

    if crossover_nnz is None:
        cross = find_crossover(records)
        crossover_nnz = cross["nonzeros"] if cross else None

    # Canvas dimensions
    width, height = 900, 560
    margin_l, margin_r = 90, 40
    margin_t, margin_b = 70, 70
    plot_w = width - margin_l - margin_r
    plot_h = height - margin_t - margin_b

    # Log ranges (x = nonzeros, y = wall-clock ms)
    min_x = min(r["nonzeros"] for r in valid_records) / 2.5
    max_x = max(r["nonzeros"] for r in valid_records) * 2.5
    min_y = 0.01   # 0.01 ms = 10 microseconds
    solved_times = [_solved_ms(r["series"].get(k))
                    for r in valid_records for k in SERIES_ORDER
                    if k in r["series"]]
    solved_times = [t for t in solved_times if t == t]
    data_max = max(solved_times) if solved_times else 0.0
    max_y = 50000.0  # 50,000 ms = 50 seconds (historical default)
    if data_max > max_y:
        max_y = 10.0 ** math.ceil(math.log10(data_max * 1.5))

    def to_x_coord(val: float) -> float:
        ratio = (math.log10(val) - math.log10(min_x)) / \
            (math.log10(max_x) - math.log10(min_x))
        return margin_l + ratio * plot_w

    def to_y_coord(val: float) -> float:
        ratio = (math.log10(val) - math.log10(min_y)) / \
            (math.log10(max_y) - math.log10(min_y))
        return height - margin_b - ratio * plot_h

    def x_label(val: int) -> str:
        if val >= 1_000_000 and val % 1_000_000 == 0:
            return "%dM" % (val // 1_000_000)
        if val >= 1_000_000:
            return "%.1fM" % (val / 1_000_000)
        if val >= 1_000 and val % 1_000 == 0:
            return "%dk" % (val // 1_000)
        if val >= 1_000:
            return "%.1fk" % (val / 1_000)
        return str(val)

    def y_label(exp10: int) -> str:
        if exp10 <= 2:
            return "%g ms" % (10.0 ** exp10)
        return "%g s" % (10.0 ** (exp10 - 3))

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
        f'width="{width}" height="{height}" '
        f'style="background:#0f172a; font-family: -apple-system, BlinkMacSystemFont, Segoe UI, '
        f'Roboto, Helvetica, Arial, sans-serif;">',
        '<defs>',
        '  <linearGradient id="grid-grad" x1="0" y1="0" x2="1" y2="0">',
        '    <stop offset="0%" stop-color="#334155" stop-opacity="0.3"/>',
        '    <stop offset="100%" stop-color="#334155" stop-opacity="0.1"/>',
        '  </linearGradient>',
        '</defs>',
    ]

    # Title & Subtitle (any missing/unmeasured series is called out in the footnote)
    subtitle = ("Wall-Clock Time vs Problem Size (nonzeros): CPU Simplex vs CPU PDLP vs "
                "GPU PDLP (D-GPU-08 / SIH26119)")
    absent = [k for k in SERIES_ORDER
              if not any(k in r["series"] for r in records)]
    svg.append(
        f'<text x="{width/2}" y="32" text-anchor="middle" fill="#f8fafc" '
        f'font-size="18" font-weight="bold">markov-cero: End-to-End Scale Crossover Study</text>'
    )
    svg.append(
        f'<text x="{width/2}" y="52" text-anchor="middle" fill="#94a3b8" '
        f'font-size="13">{subtitle}</text>'
    )

    # Gridlines Y (powers of 10)
    exp_lo = int(math.floor(math.log10(min_y)))
    exp_hi = int(math.ceil(math.log10(max_y)))
    for exp10 in range(exp_lo, exp_hi + 1):
        val = 10.0 ** exp10
        if val > max_y:
            break
        y_pos = to_y_coord(val)
        svg.append(
            f'<line x1="{margin_l}" y1="{y_pos:.1f}" x2="{width - margin_r}" y2="{y_pos:.1f}" '
            f'stroke="#334155" stroke-width="1" stroke-dasharray="3,3"/>'
        )
        svg.append(
            f'<text x="{margin_l - 12}" y="{y_pos + 4:.1f}" text-anchor="end" fill="#64748b" '
            f'font-size="11">{y_label(exp10)}</text>'
        )

    # Gridlines X (decades with 1/2/5 minor steps, compact k/M labels)
    tick_values = []
    for decade in range(int(math.floor(math.log10(min_x))),
                        int(math.ceil(math.log10(max_x))) + 1):
        for mult in (1, 2, 5):
            val = int(mult * 10 ** decade)
            if min_x <= val <= max_x:
                tick_values.append(val)
    for val in tick_values:
        x_pos = to_x_coord(val)
        svg.append(
            f'<line x1="{x_pos:.1f}" y1="{margin_t}" x2="{x_pos:.1f}" '
            f'y2="{height - margin_b}" stroke="#334155" stroke-width="1" '
            f'stroke-dasharray="3,3"/>'
        )
        svg.append(
            f'<text x="{x_pos:.1f}" y="{height - margin_b + 20}" text-anchor="middle" '
            f'fill="#64748b" font-size="11">{x_label(val)}</text>'
        )

    # Axis labels
    svg.append(
        f'<text x="{width/2}" y="{height - 18}" text-anchor="middle" fill="#cbd5e1" '
        f'font-size="13" font-weight="500">Problem Size: Nonzeros (log scale)</text>'
    )
    svg.append(
        f'<text x="25" y="{height/2}" text-anchor="middle" fill="#cbd5e1" '
        f'font-size="13" font-weight="500" transform="rotate(-90 25 {height/2})">'
        f'Wall-Clock Time (ms, log scale)</text>'
    )

    # Crossover line N*
    min_nnz = min(r["nonzeros"] for r in valid_records)
    at_or_below = crossover_nnz is not None and crossover_nnz <= min_nnz
    if crossover_nnz and min_x <= crossover_nnz <= max_x:
        x_cross = to_x_coord(crossover_nnz)
        svg.append(
            f'<line x1="{x_cross:.1f}" y1="{margin_t}" x2="{x_cross:.1f}" '
            f'y2="{height - margin_b}" stroke="#f59e0b" stroke-width="2" '
            f'stroke-dasharray="6,4"/>'
        )
        badge_w = 240
        badge_x = min(max(x_cross - badge_w / 2, margin_l),
                      width - margin_r - badge_w)
        rel = "&#8804;" if at_or_below else "≈"
        svg.append(
            f'<rect x="{badge_x:.1f}" y="{margin_t + 10}" width="{badge_w}" height="24" '
            f'rx="4" fill="#78350f" stroke="#f59e0b" stroke-width="1"/>'
        )
        svg.append(
            f'<text x="{badge_x + badge_w / 2:.1f}" y="{margin_t + 26}" text-anchor="middle" '
            f'fill="#fef3c7" font-size="11" font-weight="bold">'
            f'Crossover N* {rel} {crossover_nnz:,} nonzeros</text>'
        )

    # Regime backgrounds
    if crossover_nnz is None:
        svg.append(
            f'<text x="{margin_l + 8}" y="{margin_t + 55}" fill="#94a3b8" '
            f'font-size="11" font-style="italic">'
            f'CPU Simplex leads at every measured scale</text>'
        )
    elif at_or_below:
        svg.append(
            f'<text x="{margin_l + 8}" y="{margin_t + 55}" fill="#94a3b8" '
            f'font-size="11" font-style="italic">'
            f'PDLP leads at every measured scale (N* ≤ smallest instance)</text>'
        )
    else:
        svg.append(
            f'<text x="{margin_l + 8}" y="{margin_t + 55}" fill="#94a3b8" '
            f'font-size="11" font-style="italic">Simplex Regime (N &lt; N*)</text>'
        )
    svg.append(
        f'<text x="{width - margin_r - 8}" y="{margin_t + 55}" text-anchor="end" '
        f'fill="#38bdf8" font-size="11" font-style="italic">'
        f'First-Order PDLP Regime (N ≥ N*)</text>'
    )

    # Build polylines / points (only solver-verified Optimal runs are plotted)
    drawn_series = []
    for key in SERIES_ORDER:
        style = SERIES_STYLE[key]
        pts = []
        for rec in valid_records:
            entry = rec["series"].get(key)
            ms = _solved_ms(entry)
            if ms == ms and ms > 0 and rec["nonzeros"] > 0:
                pts.append((to_x_coord(rec["nonzeros"]), to_y_coord(ms),
                            rec["nonzeros"], ms, rec["instance"],
                            bool(entry.get("verified"))))
        if not pts:
            continue
        drawn_series.append(key)
        if len(pts) > 1:
            pts_str = " ".join(f"{x:.1f},{y:.1f}" for x, y, _, _, _, _ in pts)
            dash = f' stroke-dasharray="{style["dash"]}"' if style["dash"] else ""
            width_px = 2.5 if key != "cpu_pdlp" else 2
            svg.append(
                f'<polyline points="{pts_str}" fill="none" stroke="{style["color"]}" '
                f'stroke-width="{width_px}"{dash}/>'
            )
        for cx, cy, nnz, val, name, verified in pts:
            title = ("%s %s: %.3f ms%s" %
                     (name, style["label"].split(" (")[0], val,
                      "" if verified else " [NOT VERIFIED]"))
            svg.append(
                f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{style["radius"]}" '
                f'fill="{style["color"]}" stroke="#0f172a" stroke-width="1.5">'
                f'<title>{title}</title></circle>'
            )

    # Legend (only series that appear in the CSV; note attempted-but-unsolved)
    legend_keys = [k for k in SERIES_ORDER if any(k in r["series"] for r in records)]
    if legend_keys:
        rows_h = 30
        leg_w = 300
        leg_h = 25 + rows_h * len(legend_keys)
        leg_x, leg_y = width - margin_r - leg_w - 10, height - margin_b - leg_h - 15
        svg.append(
            f'<rect x="{leg_x}" y="{leg_y}" width="{leg_w}" height="{leg_h}" rx="6" '
            f'fill="#1e293b" stroke="#334155" stroke-width="1"/>'
        )
        for idx, key in enumerate(legend_keys):
            style = SERIES_STYLE[key]
            item_y = leg_y + 25 + idx * rows_h
            dash = f' stroke-dasharray="{style["dash"]}"' if style["dash"] else ""
            svg.append(
                f'<line x1="{leg_x + 15}" y1="{item_y}" x2="{leg_x + 45}" y2="{item_y}" '
                f'stroke="{style["color"]}" stroke-width="2.5"{dash}/>'
            )
            svg.append(
                f'<circle cx="{leg_x + 30}" cy="{item_y}" r="{style["radius"]}" '
                f'fill="{style["color"]}"/>'
            )
            suffix = "" if key in drawn_series else " (no Optimal run)"
            svg.append(
                f'<text x="{leg_x + 55}" y="{item_y + 4}" fill="#f8fafc" font-size="11">'
                f'{style["label"]}{suffix}</text>'
            )

    # Honest footnote about series that never solved / were not measured
    unsolved_notes = []
    for key in SERIES_ORDER:
        statuses = sorted({(rec["series"].get(key) or {}).get("status") or ""
                           for rec in valid_records
                           if key in rec["series"] and not _is_solved(rec["series"][key])})
        statuses = [s for s in statuses if s]
        if statuses:
            unsolved_notes.append("%s: %s" % (key, "/".join(statuses)))
    if absent:
        unsolved_notes.append("gpu_pdlp not measured (solver built without CUDA)")
    if unsolved_notes:
        svg.append(
            f'<text x="{margin_l}" y="{height - 6}" fill="#64748b" font-size="10">'
            f'not plotted: {"; ".join(unsolved_notes)}</text>'
        )

    svg.append('</svg>\n')

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, "w") as f:
        f.write("\n".join(svg))
    print(f"[+] High-resolution crossover SVG saved to {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="markov-cero Scale Crossover Plot Generator"
    )
    parser.add_argument(
        "--input", default="evidence/benchmarks/crossover_study.csv",
        help="Input crossover CSV (long schema)"
    )
    parser.add_argument(
        "--output-svg", default="evidence/benchmarks/crossover_plot.svg",
        help="Output SVG plot path"
    )
    parser.add_argument(
        "--output-evidence", default="",
        help="Optional second copy of the SVG (evidence benchmark path)"
    )
    parser.add_argument(
        "--crossover-nnz", type=int, default=None,
        help="Override the empirical crossover marker (nonzeros)"
    )
    parser.add_argument(
        "--crossover-row", type=int, default=None,
        help=argparse.SUPPRESS)  # legacy flag; rows are not in the schema
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"Error: input file {args.input} not found.", file=sys.stderr)
        sys.exit(1)

    records = parse_crossover_csv(args.input)
    if not records:
        print(f"Error: no data rows in {args.input}.", file=sys.stderr)
        sys.exit(1)
    print_ascii_summary(records)

    crossover_nnz = args.crossover_nnz
    generate_svg(records, args.output_svg, crossover_nnz=crossover_nnz)
    if args.output_evidence and args.output_evidence != args.output_svg:
        generate_svg(records, args.output_evidence, crossover_nnz=crossover_nnz)


if __name__ == "__main__":
    main()
