"""BENCH-01 contract section 2: build the preregistration before any run.

The preregistration freezes the declared subset, its order, the caps,
threads, repeats, workers, metric definitions and tolerances. It is
written once and never edited; a different subset or cap means a new
preregistration.
"""

from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

from bench01_config import (COLD_REPEAT_INDEX, DEFAULT_CAPS,
                            DEFAULT_REPEATS, DEFAULT_THREADS,
                            DEFAULT_WORKERS, MANIFEST_PATH,
                            PARENT_SLACK_S, SCHEMA_VERSION, SPLIT_ORDER,
                            TOLERANCES)

# Contract section 6, quoted rather than paraphrased.
METRICS = {
    'solved_fraction': 'declared cells whose every `ok` row reports '
                       '`Optimal` ÷ declared cells; a cell with no `ok` '
                       'row is unsolved, never excluded',
    'status_counts': 'count per status over all rows',
    'objective_disagreement': 'cells whose warm-median objective differs '
                              'from another repeat of the same cell by '
                              "more than §7's tolerance",
    'reference_agreement': "`ok` rows whose objective differs from the "
                           "harness's reference optimum by more than §7's "
                           'tolerance, per class',
    'independent_primal_failures': "`ok` rows whose harness-recomputed "
                                   "feasibility exceeds §7's tolerance",
    'time_distribution': 'cold and warm process-wall per class: n, min, '
                         'median, max, and the paired per-instance ratios '
                         'when a second solver is present',
    'rss_distribution': 'per-child peak RSS (KiB) sampled from '
                        '`/proc/<pid>/status` `VmHWM` while the child runs, '
                        'per class: n, min, median, max',
    'nodes_and_gaps': '`nodes_explored` and `relative_gap` counts as '
                      'measured, only over rows that reported them',
}

LIMITATIONS = [
    'single host: no second-host timing, so the promotion and second-host '
    'acceptance criterion of the release contract is documented as NOT MET',
    'the frozen corpus declares 265 suite instances and 23 curated ones; '
    'only the instances marked present in this checkout are executed, and '
    'every absent instance stays in the denominator as `absent`',
    'workers > 1 runs single-threaded children concurrently, so absolute '
    'wall times include interference; correctness metrics are unaffected '
    'and no sequential-versus-concurrent speed comparison is published',
    'reference optima exist for only part of the present subset, so '
    'reference_agreement covers fewer rows than solved_fraction',
]


def sha256_file(path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_instances(manifest_path: str) -> tuple:
    """Return (manifest dict, flattened declared subset)."""
    manifest = json.loads(Path(manifest_path).read_text())
    suite_class = manifest['timing']['suite_class']
    declared = []
    for suite in manifest['suites']:
        for entry in suite['instances']:
            declared.append({
                'id': entry['id'],
                'name': entry['name'],
                'suite': entry['suite'],
                'problem_class': entry.get('problem_class')
                or suite_class.get(suite['suite'], ''),
                'split': entry.get('split', 'train'),
                'family': entry.get('family', ''),
                'sha256': entry.get('sha256', ''),
                'present': bool(entry.get('present')),
            })
    return manifest, declared


def reference_objectives(declared: list) -> tuple:
    """Reference optima from provenance files and the frozen tables."""
    references, sources = {}, {}
    import re
    from importlib import util as importlib_util

    def _module(name: str, path: str):
        spec = importlib_util.spec_from_file_location(name, path)
        module = importlib_util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    netlib = getattr(_module('bench01_netlib',
                             str(Path(__file__).parents[1] /
                                 'support/run_netlib_config.py')),
                     'NETLIB_BENCHMARKS', {})
    source = Path(__file__).parents[1] / 'run_miplib.py'
    text = source.read_text()
    start = text.index('MIPLIB_BENCHMARKS = {')
    depth, end = 0, text.index('{', start)
    for index in range(end, len(text)):
        depth += (text[index] == '{') - (text[index] == '}')
        if depth == 0:
            end = index
            break
    scope = {}
    exec(text[start:end + 1], scope)
    miplib = scope['MIPLIB_BENCHMARKS']

    for cell in declared:
        if not cell['present']:
            continue
        name = Path(cell['id']).stem
        value, origin = None, None
        provenance = Path(cell['id'].replace('.mps', '.provenance.json'))
        if provenance.is_file():
            try:
                payload = json.loads(provenance.read_text())
            except (OSError, ValueError):
                payload = {}
            candidate = payload.get('reference_objective')
            if isinstance(candidate, (int, float)):
                value, origin = float(candidate), 'provenance'
        if value is None and cell['suite'] == 'netlib':
            candidate = (netlib.get(name) or {}).get('optimal')
            if isinstance(candidate, (int, float)):
                value, origin = float(candidate), 'run_netlib_config'
        if value is None:
            candidate = (miplib.get(name) or {}).get('optimal')
            if isinstance(candidate, (int, float)):
                value, origin = float(candidate), 'run_miplib'
        if value is not None:
            references[cell['id']] = value
            sources[cell['id']] = origin
    return references, sources


def execution_order(declared: list) -> list:
    return [cell['id'] for cell in sorted(
        declared, key=lambda c: (SPLIT_ORDER.get(c['split'], 9),
                                 c['id']))]


def build(campaign_id: str, binary: str, workers: int,
          manifest_path: str = MANIFEST_PATH) -> dict:
    manifest, declared = load_instances(manifest_path)
    references, sources = reference_objectives(declared)
    return {
        'schema': SCHEMA_VERSION,
        'contract': {'path': 'docs/contracts/benchmark-campaign.md',
                     'version': 1},
        'campaign_id': campaign_id,
        'written_before_any_run': True,
        'written_at': datetime.now(timezone.utc).isoformat(),
        'manifest': {'path': manifest_path, 'date': manifest['date'],
                     'sha256': sha256_file(manifest_path)},
        'declared_subset': declared,
        'absent': [c['id'] for c in declared if not c['present']],
        'present_count': sum(1 for c in declared if c['present']),
        'declared_count': len(declared),
        'order': execution_order(declared),
        'caps': dict(DEFAULT_CAPS),
        'parent_slack_s': PARENT_SLACK_S,
        'threads': DEFAULT_THREADS,
        'repeats': {'per_cell': DEFAULT_REPEATS,
                    'cold_index': COLD_REPEAT_INDEX},
        'workers': {
            'count': workers,
            'interference_caveat':
                'absolute wall times include interference between '
                'concurrent single-threaded children; correctness metrics '
                'are unaffected and no speed comparison is published from '
                'this campaign',
        },
        'timing_scope': ('process_wall: parent-observed wall clock around '
                         'the solver process, including model parse, CLI '
                         'start-up and result serialisation'),
        'metrics': METRICS,
        'tolerances': TOLERANCES,
        'reference_objectives': references,
        'reference_sources': sources,
        'reference_coverage': {'with_reference': len(references),
                               'present': sum(1 for c in declared
                                              if c['present'])},
        'known_limitations': LIMITATIONS,
        'solver': {'path': str(binary), 'sha256': sha256_file(binary)},
        'hardware_record': 'evidence/hardware.md',
    }


def write(payload: dict, path) -> Path:
    target = Path(path)
    if target.exists():
        raise SystemExit(f'refusing to overwrite an existing '
                         f'preregistration: {target}')
    target.write_text(json.dumps(payload, indent=2, sort_keys=False) + '\n')
    return target
