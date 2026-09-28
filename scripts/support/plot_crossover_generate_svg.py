from .plot_crossover_config import (
    Any, Dict, List, Optional, SERIES_ORDER, SERIES_STYLE, math, os, sys
)
from .plot_crossover_print_ascii_summary import _is_solved
from .plot_crossover_print_ascii_summary import _solved_ms
from .plot_crossover_print_ascii_summary import find_crossover

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
