from __future__ import annotations
from .dolan_more_profile_config import (
    FALLBACK_COLORS, SOLVER_ORDER, SOLVER_STYLE, csv, math, os
)

def load_results(path: str):
    """Read full_compare_results.csv -> (problems, solvers, success, runtime)."""
    problems = []
    solvers = []
    success = {}
    runtime = {}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            inst = (row.get("instance") or "").strip()
            sol = (row.get("solver") or "").strip()
            if not inst or not sol:
                continue
            if inst not in problems:
                problems.append(inst)
            if sol not in solvers:
                solvers.append(sol)
            ok = (row.get("status") or "").strip() == "Optimal"
            try:
                t = float(row.get("runtime_ms") or "nan")
            except ValueError:
                t = float("nan")
            success[(inst, sol)] = ok
            runtime[(inst, sol)] = t
    ordered = [s for s in SOLVER_ORDER if s in solvers] + \
              [s for s in solvers if s not in SOLVER_ORDER]
    return problems, ordered, success, runtime

def performance_ratios(problems, solvers, success, runtime):
    """Exact r_{p,s}; failures are math.inf."""
    ratios = {}
    for p in problems:
        best = math.inf
        for s in solvers:
            if success.get((p, s)):
                t = runtime.get((p, s), math.nan)
                if math.isfinite(t) and t >= 0:
                    best = min(best, t)
        for s in solvers:
            if success.get((p, s)):
                t = runtime.get((p, s), math.nan)
                if math.isfinite(t) and t >= 0 and best > 0:
                    ratios[(p, s)] = t / best if best > 0 else 1.0
                else:
                    ratios[(p, s)] = math.inf
            else:
                ratios[(p, s)] = math.inf
    return ratios

def rho_value(problems, ratios, solver, tau):
    if not problems:
        return 0.0
    count = 0
    for p in problems:
        r = ratios.get((p, solver), math.inf)
        if r <= tau:
            count += 1
    return count / len(problems)

def log_grid(lo, hi, n):
    if n <= 1:
        return [lo]
    out = []
    for i in range(n):
        frac = i / (n - 1)
        out.append(math.exp(math.log(lo) + frac * (math.log(hi) - math.log(lo))))
    out[0] = lo
    out[-1] = hi
    return out

def polyline_samples(problems, ratios, solver, tau_max, grid):
    """Grid points plus exact breakpoints so the step function is drawn exactly."""
    pts = list(grid)
    for p in problems:
        r = ratios.get((p, solver), math.inf)
        if math.isfinite(r) and 1.0 <= r <= tau_max:
            pts.append(r)
            pts.append(r * (1.0 - 1e-9))
            if r < tau_max:
                pts.append(r * (1.0 + 1e-9))
    pts = sorted({round(v, 12) for v in pts if 1.0 <= v <= tau_max})
    return pts

def write_profile_csv(path, problems, solvers, ratios, grid):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["tau", "solver", "rho"])
        for tau in grid:
            for s in solvers:
                w.writerow([f"{tau:.6f}", s, f"{rho_value(problems, ratios, s, tau):.6f}"])
    return path

def generate_svg(path, problems, solvers, ratios, grid, tau_max=100.0):
    width, height = 900, 560
    margin_l, margin_r = 90, 40
    margin_t, margin_b = 78, 70
    plot_w = width - margin_l - margin_r
    plot_h = height - margin_t - margin_b

    def to_x(tau):
        frac = (math.log10(tau) - 0.0) / (math.log10(tau_max) - 0.0)
        return margin_l + frac * plot_w

    def to_y(rho):
        return height - margin_b - rho * plot_h

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
        f'width="{width}" height="{height}" '
        f'style="background:#0f172a; font-family: -apple-system, BlinkMacSystemFont, '
        f'Segoe UI, Roboto, Helvetica, Arial, sans-serif;">',
        '<defs>',
        '  <linearGradient id="grid-grad" x1="0" y1="0" x2="1" y2="0">',
        '    <stop offset="0%" stop-color="#334155" stop-opacity="0.3"/>',
        '    <stop offset="100%" stop-color="#334155" stop-opacity="0.1"/>',
        '  </linearGradient>',
        '</defs>',
    ]

    svg.append(
        f'<text x="{width / 2}" y="30" text-anchor="middle" fill="#f8fafc" '
        f'font-size="18" font-weight="bold">Dolan-Mor&eacute; Performance Profile '
        f'&mdash; Runtime</text>'
    )
    svg.append(
        f'<text x="{width / 2}" y="52" text-anchor="middle" fill="#94a3b8" '
        f'font-size="13">{len(problems)} curated problems &middot; '
        f'{len(solvers)} solvers &middot; r_{{p,s}} = t_{{p,s}} / min_s&#39; t_{{p,s&#39;}} '
        f'&middot; &#961;_s(&#964;) = |&#123;p : r_{{p,s}} &#8804; &#964;&#125;| / |P|</text>'
    )

    y_ticks = [i / 5.0 for i in range(0, 6)]
    for val in y_ticks:
        y_pos = to_y(val)
        svg.append(
            f'<line x1="{margin_l}" y1="{y_pos:.1f}" x2="{width - margin_r}" '
            f'y2="{y_pos:.1f}" stroke="#334155" stroke-width="1" stroke-dasharray="3,3"/>'
        )
        svg.append(
            f'<text x="{margin_l - 12}" y="{y_pos + 4:.1f}" text-anchor="end" '
            f'fill="#64748b" font-size="11">{val:.1f}</text>'
        )

    x_candidates = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000]
    x_ticks = [v for v in x_candidates if v <= tau_max]
    if 1 not in x_ticks:
        x_ticks = [1] + x_ticks
    for val in x_ticks:
        x_pos = to_x(val)
        svg.append(
            f'<line x1="{x_pos:.1f}" y1="{margin_t}" x2="{x_pos:.1f}" '
            f'y2="{height - margin_b}" stroke="#334155" stroke-width="1" '
            f'stroke-dasharray="3,3"/>'
        )
        svg.append(
            f'<text x="{x_pos:.1f}" y="{height - margin_b + 20}" text-anchor="middle" '
            f'fill="#64748b" font-size="11">{val}&#215;</text>'
        )

    svg.append(
        f'<text x="{width / 2}" y="{height - 18}" text-anchor="middle" fill="#cbd5e1" '
        f'font-size="13" font-weight="500">Performance ratio &#964; '
        f'(solver time / best time, log scale)</text>'
    )
    svg.append(
        f'<text x="25" y="{height / 2}" text-anchor="middle" fill="#cbd5e1" '
        f'font-size="13" font-weight="500" transform="rotate(-90 25 {height / 2})">'
        f'&#961;_s(&#964;) &mdash; fraction of problems solved within &#964; &#215; best</text>'
    )

    legend_items = []
    for idx, s in enumerate(solvers):
        color, dash, label = SOLVER_STYLE.get(
            s, (FALLBACK_COLORS[idx % len(FALLBACK_COLORS)], "none", s)
        )
        xs = polyline_samples(problems, ratios, s, tau_max, grid)
        pts = " ".join(f"{to_x(t):.2f},{to_y(rho_value(problems, ratios, s, t)):.2f}"
                       for t in xs)
        dash_attr = f' stroke-dasharray="{dash}"' if dash and dash != "none" else ""
        svg.append(
            f'<polyline points="{pts}" fill="none" stroke="{color}" '
            f'stroke-width="2.5"{dash_attr} stroke-linejoin="round"/>'
        )
        legend_items.append((s, color, dash, label,
                             rho_value(problems, ratios, s, 1.0),
                             rho_value(problems, ratios, s, tau_max)))

    rows = len(legend_items)
    leg_h = 34 + 24 * rows
    leg_w = 320
    leg_x, leg_y = width - margin_r - leg_w - 10, margin_t + 8
    svg.append(
        f'<rect x="{leg_x}" y="{leg_y}" width="{leg_w}" height="{leg_h}" rx="6" '
        f'fill="#1e293b" stroke="#334155" stroke-width="1"/>'
    )
    svg.append(
        f'<text x="{leg_x + 14}" y="{leg_y + 21}" fill="#94a3b8" font-size="11" '
        f'font-weight="bold">&#961;_s(1) / &#961;_s({int(tau_max)})</text>'
    )
    for i, (name, color, dash, label, r1, rn) in enumerate(legend_items):
        y = leg_y + 40 + 24 * i
        dash_attr = f' stroke-dasharray="{dash}"' if dash and dash != "none" else ""
        svg.append(
            f'<line x1="{leg_x + 14}" y1="{y - 4}" x2="{leg_x + 52}" y2="{y - 4}" '
            f'stroke="{color}" stroke-width="2.5"{dash_attr}/>'
        )
        svg.append(
            f'<text x="{leg_x + 60}" y="{y}" fill="#f8fafc" font-size="11">'
            f'{label}  ({r1:.2f} / {rn:.2f})</text>'
        )

    svg.append('</svg>\n')
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(svg))
    return path
