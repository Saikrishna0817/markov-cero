"""BENCH-01 contract section 6: metrics from the raw rows.

Every metric carries its numerator, its denominator and — when it drops
rows — the run states it dropped. Nothing here reads a solver status
back into a row; the summariser only counts what the rows say.
"""

from __future__ import annotations

import statistics
from collections import Counter, defaultdict


def _num(value) -> float | None:
    if isinstance(value, bool) or value is None or value == '':
        return None
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if number == number else None


def _is_optimal(row: dict) -> bool:
    return row.get('run_state') == 'ok' and row.get('status') == 'Optimal'


def _cells(rows: list) -> dict:
    grouped = defaultdict(list)
    for row in rows:
        grouped[row['instance']].append(row)
    return grouped


def _ratio(numerator: int, denominator: int) -> dict:
    return {'numerator': numerator, 'denominator': denominator,
            'value': (numerator / denominator) if denominator else None}


def _spread(values: list) -> dict:
    if not values:
        return {'n': 0, 'min': None, 'median': None, 'max': None}
    return {'n': len(values), 'min': min(values),
            'median': statistics.median(values), 'max': max(values)}


def solved_fraction(rows: list, declared: list) -> dict:
    """Cells whose every ``ok`` row is Optimal, over the declared cells."""
    grouped = _cells(rows)
    solved = 0
    for cell in declared:
        ok = [r for r in grouped.get(cell['id'], [])
              if r.get('run_state') == 'ok']
        if ok and all(r.get('status') == 'Optimal' for r in ok):
            solved += 1
    result = _ratio(solved, len(declared))
    result['rule'] = ('a declared cell is solved when every row it emitted '
                      'with run_state=ok reports status=Optimal and at least '
                      'one such row exists; cells with no ok row are '
                      'unsolved, not excluded')
    return result


def objective_disagreement(rows: list, tolerance: float) -> dict:
    """Cells whose warm objectives disagree by more than T4."""
    disagreements, eligible = [], 0
    for instance, group in sorted(_cells(rows).items()):
        warm = [r for r in group if r.get('cold_warm') == 'warm'
                and r.get('run_state') == 'ok']
        values = [_num(r.get('objective')) for r in warm]
        values = [v for v in values if v is not None]
        if len(values) < 2:
            continue
        eligible += 1
        spread = max(values) - min(values)
        if spread > tolerance * max(1.0, max(abs(v) for v in values)):
            disagreements.append({'instance': instance,
                                  'spread': spread,
                                  'objectives': values})
    return {'tolerance': tolerance, 'disagreements': disagreements,
            **_ratio(len(disagreements), eligible)}


def reference_agreement(rows: list, references: dict, tolerances: dict) -> dict:
    per_class, totals = {}, {'compared': 0, 'disagreements': 0}
    for row in rows:
        if row.get('run_state') != 'ok':
            continue
        reference = references.get(row['instance'])
        objective = _num(row.get('objective'))
        if reference is None or objective is None:
            continue
        klass = row.get('problem_class', '')
        tolerance = tolerances.get(klass, 1e-4)
        allowance = tolerance * max(1.0, abs(reference))
        bucket = per_class.setdefault(klass, {'compared': 0,
                                              'disagreements': 0,
                                              'tolerance': tolerance})
        bucket['compared'] += 1
        totals['compared'] += 1
        if abs(objective - reference) > allowance:
            bucket['disagreements'] += 1
            totals['disagreements'] += 1
    for bucket in per_class.values():
        bucket.update(_ratio(bucket['disagreements'], bucket['compared']))
        bucket['denominator_note'] = 'ok rows whose instance holds a reference'
    per_class['all'] = {**_ratio(totals['disagreements'],
                                 totals['compared']),
                        'denominator_note': 'ok rows whose instance holds a reference'}
    return per_class


def independent_primal(rows: list, tolerances: dict) -> dict:
    counts = Counter(r.get('independent_check', '') for r in rows
                     if r.get('run_state') == 'ok')
    checked = counts.get('pass', 0) + counts.get('fail', 0)
    result = _ratio(counts.get('fail', 0), checked)
    result['counts'] = dict(counts)
    result['tolerances'] = {k: tolerances[k] for k in
                            ('row_feasibility', 'bound_feasibility',
                             'integrality')}
    result['excluded'] = {'not_checked': counts.get('not_checked', 0),
                          'run_states': []}
    return result


def time_and_rss(rows: list) -> dict:
    times, rss = defaultdict(list), defaultdict(list)
    for row in rows:
        if row.get('run_state') != 'ok':
            continue
        wall = _num(row.get('parent_wall_ms'))
        if wall is not None:
            times[(row.get('problem_class', ''), row.get('cold_warm', ''))].append(wall)
        peak = _num(row.get('peak_rss_kib'))
        if peak is not None:
            rss[row.get('problem_class', '')].append(peak)
    return {
        'timing_scope': 'process_wall, parent-observed',
        'time_distribution': {
            f'{klass}:{phase}': _spread(values)
            for (klass, phase), values in sorted(times.items())},
        'rss_distribution_kib': {klass: _spread(values)
                                 for klass, values in sorted(rss.items())},
    }


def nodes_and_gaps(rows: list) -> dict:
    nodes, gaps = [], []
    for row in rows:
        if row.get('run_state') != 'ok':
            continue
        value = _num(row.get('nodes'))
        if value is not None:
            nodes.append(value)
        gap = _num(row.get('relative_gap'))
        if gap is not None:
            gaps.append(gap)
    return {'rows_reporting_nodes': len(nodes),
            'rows_reporting_relative_gap': len(gaps),
            'max_nodes_explored': max(nodes) if nodes else None,
            'relative_gap_distribution': _spread(gaps)}


def summarise(rows: list, declared: list, references: dict,
              tolerances: dict) -> dict:
    ok_rows = [r for r in rows if r.get('run_state') == 'ok']
    grouped = _cells(rows)
    disagreements, comparable = 0, 0
    for cell in declared:
        statuses = {r.get('status') for r in grouped.get(cell['id'], [])
                    if r.get('run_state') == 'ok'}
        if statuses:
            comparable += 1
            if len(statuses) > 1:
                disagreements += 1
    return {
        'declared_cells': len(declared),
        'rows_emitted': len(rows),
        'run_state_counts': dict(Counter(r.get('run_state', '')
                                          for r in rows)),
        'status_counts_all_rows': dict(Counter(r.get('status', '')
                                               for r in rows)),
        'status_counts_ok_rows': dict(Counter(r.get('status', '')
                                              for r in ok_rows)),
        'cold_warm_counts': dict(Counter(r.get('cold_warm', '')
                                         for r in rows)),
        'solved_fraction': solved_fraction(rows, declared),
        'cells_status_disagreement': _ratio(disagreements, comparable),
        'objective_disagreement': objective_disagreement(
            rows, tolerances['objective_repeat']),
        'reference_agreement': reference_agreement(
            rows, references, tolerances['objective_reference']),
        'independent_primal_failures': independent_primal(rows, tolerances),
        **time_and_rss(rows),
        'nodes_and_gaps': nodes_and_gaps(rows),
    }
