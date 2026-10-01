#!/usr/bin/env python3
"""BENCH-01 campaign driver — docs/contracts/benchmark-campaign.md v1.

Subcommands: preregister (write the immutable declaration), run (emit one
row per declared cell x repeat), summarise (contract section 6 metrics).
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import sys
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / 'support'))

from bench01_config import (COLUMNS, DEFAULT_THREADS, MANIFEST_PATH,
                            SCHEMA_VERSION)  # noqa: E402
import bench01_prereg as prereg  # noqa: E402
from bench01_runner import run_once  # noqa: E402
from bench01_summarise import summarise  # noqa: E402

LOCK = threading.Lock()
TOLERANCES = prereg.TOLERANCES


def _numeric(text):
    try:
        return float(text)
    except (TypeError, ValueError):
        return None


def _blank_row(entry: dict, campaign_id: str, manifest_date: str,
               solver_sha: str, repeat: int, cap_s) -> dict:
    return {column: '' for column in COLUMNS} | {
        'campaign_id': campaign_id, 'manifest_version': manifest_date,
        'solver': entry['solver_path'], 'solver_sha256': solver_sha,
        'instance': entry['id'], 'suite': entry['suite'],
        'problem_class': entry['problem_class'], 'split': entry['split'],
        'repeat': repeat,
        'cold_warm': 'cold' if repeat == 0 else 'warm',
        'threads': entry['threads'], 'cap_s': cap_s,
    }


def _independent_check(entry: dict, record: dict) -> tuple:
    if record.get('run_state') != 'ok':
        return '', ''
    try:
        import bench01_mps
        model = bench01_mps.parse(Path(entry['id']))
    except Exception as exc:  # unparsable model is not_checked, never pass
        return 'not_checked', f'model_unreadable:{type(exc).__name__}'
    verdict, violation, note = bench01_mps.check(
        model, record.get('primal'), record.get('variable_names'),
        TOLERANCES['row_feasibility'], TOLERANCES['bound_feasibility'],
        TOLERANCES['integrality'])
    return verdict, f'{violation:.6e}' if verdict != 'not_checked' else note


def _finish_row(entry: dict, record: dict, campaign_id: str,
                manifest_date: str, solver_sha: str, repeat: int,
                cap_s, reference) -> dict:
    row = _blank_row(entry, campaign_id, manifest_date, solver_sha,
                     repeat, cap_s)
    row.update({
        'run_state': record.get('run_state', ''),
        'parent_wall_ms': record.get('parent_wall_ms', ''),
        'solver_runtime_ms': record.get('solver_runtime_ms', ''),
        'status': record.get('status', ''),
        'verified': ('' if record.get('verified') in (None, '')
                     else str(bool(record.get('verified'))).lower()),
        'assurance': record.get('assurance', ''),
        'objective': record.get('objective', ''),
        'best_bound': record.get('best_bound', ''),
        'relative_gap': record.get('relative_gap', ''),
        'nodes': record.get('nodes_explored', ''),
        'lp_iterations': record.get('lp_iterations', ''),
        'peak_rss_kib': record.get('peak_rss_kib', ''),
        'notes': record.get('notes', ''),
    })
    if reference is not None:
        row['reference_objective'] = reference
        objective = _numeric(row['objective'])
        if objective is not None:
            row['objective_delta'] = abs(objective - reference)
    verdict, detail = _independent_check(entry, record)
    row['independent_check'] = verdict
    row['independent_max_violation'] = detail
    if verdict == 'fail' and str(record.get('verified')).lower() == 'true':
        row['notes'] = (row['notes'] + '; ' if row['notes'] else '') + \
            'harness_feasibility_failure_solver_reported_verified'
    return row


def _execute(entry: dict, repeat: int, campaign_id: str,
             manifest_date: str, solver_sha: str, preregistration: dict) -> dict:
    cap_s = preregistration['caps'][entry['problem_class']]
    reference = preregistration['reference_objectives'].get(entry['id'])

    def finish(record: dict) -> dict:
        return _finish_row(entry, record, campaign_id, manifest_date,
                           solver_sha, repeat, cap_s, reference)

    if not entry['present']:
        return finish({'run_state': 'absent', 'notes': 'model_not_in_checkout'})
    if preregistration['solver']['sha256'] != solver_sha:
        return finish({'run_state': 'binary_mutated',
                       'notes': 'binary_sha_changed'})
    model_path = Path(entry['id'])
    if model_path.is_file() and entry['sha256'] and \
            prereg.sha256_file(model_path) != entry['sha256']:
        return finish({'run_state': 'hash_mismatch',
                       'notes': 'model_sha256_disagrees'})
    record = run_once(preregistration['solver']['path'], entry['id'],
                      entry['threads'], cap_s,
                      preregistration['parent_slack_s'])
    if entry['threads'] != preregistration['threads']:
        record['run_state'] = 'thread_mismatch'
        record['notes'] = (record.get('notes', '') + '; ' if record.get('notes')
                           else '') + 'threads_differ_from_preregistration'
    return finish(record)


def command_preregister(args) -> int:
    payload = prereg.build(args.campaign_id, args.binary, args.workers,
                           args.manifest)
    target = prereg.write(payload, args.out)
    print(f'wrote {target} before any run: '
          f'{payload["declared_count"]} declared, '
          f'{payload["present_count"]} present, '
          f'{len(payload["reference_objectives"])} with reference optima')
    return 0


def _read_rows(path: Path) -> list:
    if not path.is_file():
        return []
    return list(csv.DictReader(path.open(newline='')))


def command_run(args) -> int:
    preregistration = json.loads(Path(args.prereg).read_text())
    if preregistration.get('written_before_any_run') is not True:
        raise SystemExit('preregistration does not declare '
                         'written_before_any_run=true')
    solver_path = preregistration['solver']['path']
    if prereg.sha256_file(solver_path) != preregistration['solver']['sha256']:
        raise SystemExit('solver binary does not match the preregistration')
    entries = {cell['id']: cell for cell in preregistration['declared_subset']}
    for cell in entries.values():
        cell['threads'] = args.threads
        cell['solver_path'] = solver_path
    repeats = preregistration['repeats']['per_cell']
    csv_path = Path(args.csv)
    done = set()
    if csv_path.is_file():
        if not args.resume:
            raise SystemExit(f'{csv_path} exists; pass --resume to continue')
        for row in _read_rows(csv_path):
            if row.get('solver_sha256') != preregistration['solver']['sha256'] \
                    or row.get('threads') != str(args.threads) \
                    or row.get('cap_s') not in {
                        str(v) for v in preregistration['caps'].values()} \
                    or row.get('manifest_version') != \
                    preregistration['manifest']['date']:
                raise SystemExit('existing rows disagree with the '
                                 'preregistration; refusing to resume')
            done.add((row.get('instance'), row.get('repeat')))
    else:
        with csv_path.open('w', newline='') as handle:
            csv.DictWriter(handle, fieldnames=COLUMNS).writeheader()
    tasks = [(instance, repeat)
             for repeat in range(repeats)
             for instance in preregistration['order']
             if (instance, str(repeat)) not in done]
    print(f'{len(tasks)} rows to emit '
          f'({len(done)} already present), workers='
          f'{preregistration["workers"]["count"]}')
    campaign_id = preregistration['campaign_id']
    manifest_date = preregistration['manifest']['date']
    solver_sha = preregistration['solver']['sha256']
    written, failed = 0, []

    def emit(row: dict) -> None:
        nonlocal written
        with LOCK:
            with csv_path.open('a', newline='') as handle:
                csv.DictWriter(handle, fieldnames=COLUMNS).writerow(row)
            written += 1
            if written % 25 == 0:
                print(f'  {written}/{len(tasks)} rows', flush=True)

    def work(task) -> None:
        instance, repeat = task
        try:
            emit(_execute(entries[instance], int(repeat), campaign_id,
                          manifest_date, solver_sha, preregistration))
        except Exception as exc:  # a crash still emits a row
            row = _blank_row(entries[instance], campaign_id, manifest_date,
                             solver_sha, int(repeat), None)
            row['run_state'] = 'solver_error'
            row['status'] = 'ParseError'
            row['notes'] = f'harness_exception:{type(exc).__name__}:{exc}'
            emit(row)
            failed.append(f'{instance}:{repeat}')

    workers = preregistration['workers']['count']
    with ThreadPoolExecutor(max_workers=workers) as pool:
        # Rounds (contract section 4.4): index 0 finishes before index 1
        # starts, so a cell never runs two of its own repeats at once.
        for repeat in range(repeats):
            batch = [task for task in tasks if task[1] == repeat]
            for future in as_completed([pool.submit(work, task)
                                        for task in batch]):
                future.result()
    mutated = prereg.sha256_file(solver_path) != solver_sha
    print(f'done: {written} rows written, harness exceptions: {len(failed)}'
          f'{", binary_mutated" if mutated else ""}')
    return 1 if mutated or failed else 0


def command_summarise(args) -> int:
    preregistration = json.loads(Path(args.prereg).read_text())
    rows = _read_rows(Path(args.csv))
    for row in rows:
        for key in ('parent_wall_ms', 'objective', 'best_bound',
                    'relative_gap', 'peak_rss_kib', 'reference_objective',
                    'objective_delta'):
            row[key] = _numeric(row[key]) if row.get(key) not in (None, '') \
                else row.get(key)
    summary = summarise(rows, preregistration['declared_subset'],
                        preregistration['reference_objectives'],
                        preregistration['tolerances'])
    solver_path = preregistration['solver']['path']
    payload = {
        'schema': SCHEMA_VERSION,
        'campaign_id': preregistration['campaign_id'],
        'preregistration': args.prereg,
        'preregistration_sha256': prereg.sha256_file(args.prereg),
        'host': {'platform': platform.platform(), 'machine': platform.machine(),
                 'processor': platform.processor(),
                 'python': platform.python_version(),
                 'cpu_count': os.cpu_count(), 'solver_path': solver_path,
                 'solver_sha256': prereg.sha256_file(solver_path)},
        'raw_csv': str(args.csv),
        'raw_csv_sha256': prereg.sha256_file(args.csv),
        'complete': True,
        'known_limitations': preregistration['known_limitations'],
        'summary': summary,
    }
    Path(args.out).write_text(json.dumps(payload, indent=2, default=str) + '\n')
    print(json.dumps({k: summary[k] for k in
                      ('declared_cells', 'rows_emitted', 'run_state_counts',
                       'solved_fraction', 'independent_primal_failures')},
                     indent=2, default=str))
    print(f'wrote {args.out}')
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    specs = (
        ('preregister', 'write the declaration',
         ('campaign-id', 'binary', 'out'),
         (('workers', 4), ('manifest', MANIFEST_PATH))),
        ('run', 'emit one row per cell x repeat',
         ('prereg', 'csv'), (('threads', DEFAULT_THREADS), ('resume', False))),
        ('summarise', 'contract section 6 metrics',
         ('prereg', 'csv', 'out'), ()),
    )
    for name, help_text, required, optional in specs:
        p = sub.add_parser(name, help=help_text)
        for flag in required:
            p.add_argument(f'--{flag}', required=True)
        for flag, default in optional:
            if isinstance(default, bool):
                p.add_argument(f'--{flag}', action='store_true')
            elif isinstance(default, int):
                p.add_argument(f'--{flag}', type=int, default=default)
            else:
                p.add_argument(f'--{flag}', default=default)
    for name, _, _, _ in specs:
        sub.choices[name].set_defaults(
            func={'preregister': command_preregister, 'run': command_run,
                  'summarise': command_summarise}[name])
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == '__main__':
    raise SystemExit(main())
