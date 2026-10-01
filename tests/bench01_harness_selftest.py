#!/usr/bin/env python3
"""BENCH-01 contract section 10 — harness self-test.

Every record below is fabricated inside this file. No fabricated record
is ever written into `evidence/`: the model fixture and the row builder
exercise live only in a temporary directory.
"""

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'scripts' / 'support'))

from bench01_config import (COLUMNS, RUN_STATES,  # noqa: E402
                            SELF_TEST_OBLIGATIONS, TOLERANCES)
from bench01_summarise import summarise  # noqa: E402

import run_bench01_campaign as campaign  # noqa: E402

COLD = TOLERANCES['objective_repeat']

TINY_MPS = """NAME          SELFTEST
ROWS
 N  OBJ
 G  R1
COLUMNS
    X1        OBJ                        1
    X1        R1                         1
RHS
    RHS1      R1                         1
BOUNDS
 UP BND       X1                         1
ENDATA
"""


def fabricated_row(instance, klass, **overrides) -> dict:
    row = {column: '' for column in COLUMNS}
    row.update({
        'campaign_id': 'bench01-selftest', 'manifest_version': 'selftest',
        'solver': 'fabricated', 'solver_sha256': '0' * 64,
        'instance': instance, 'suite': 'fabricated',
        'problem_class': klass, 'split': 'train', 'repeat': 0,
        'cold_warm': 'cold', 'run_state': 'ok', 'threads': 1,
        'cap_s': 60, 'parent_wall_ms': 10.0, 'status': 'Optimal',
        'verified': 'false', 'assurance': 'none', 'objective': 1.0,
        'independent_check': 'pass', 'independent_max_violation': '1.0e-09',
        'notes': 'fabricated',
    })
    row.update(overrides)
    return row


def declared_cells():
    return [{'id': name, 'suite': 'fabricated', 'problem_class': klass,
             'split': 'train', 'present': True, 'sha256': '', 'name': name}
            for name, klass in (('cell_a.mps', 'LP'), ('cell_b.mps', 'MILP'),
                                ('cell_c.mps', 'LP'), ('cell_d.mps', 'MILP'),
                                ('cell_e.mps', 'QP'))]


class SelfTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.declared = declared_cells()
        cls.rows = [
            fabricated_row('cell_a.mps', 'LP', repeat=0, objective=100.0),
            fabricated_row('cell_a.mps', 'LP', repeat=1, cold_warm='warm',
                           objective=100.0),
            fabricated_row('cell_a.mps', 'LP', repeat=2, cold_warm='warm',
                           objective=100.5),
            fabricated_row('cell_b.mps', 'MILP', objective=5.0,
                           independent_check='fail', verified='true',
                           independent_max_violation='7.5e-06'),
            fabricated_row('cell_c.mps', 'LP', run_state='absent',
                           status='', objective='', verified='',
                           independent_check=''),
            fabricated_row('cell_d.mps', 'MILP', run_state='parent_timeout',
                           status='ParentTimeout', objective='',
                           verified='', independent_check=''),
            fabricated_row('cell_d.mps', 'MILP', repeat=1, cold_warm='warm',
                           run_state='solver_error', status='ParseError',
                           objective='', verified='', independent_check=''),
        ]
        cls.summary = summarise(cls.rows, cls.declared,
                                {'cell_a.mps': 100.0},
                                TOLERANCES)

    def test_obligations_are_declared(self):
        self.assertEqual(len(SELF_TEST_OBLIGATIONS), 6)
        for state in ('parent_timeout', 'solver_error', 'absent', 'ok'):
            self.assertIn(state, RUN_STATES)

    def test_timeout_and_parse_error_stay_in_denominator(self):
        counts = self.summary['run_state_counts']
        self.assertEqual(counts.get('parent_timeout'), 1)
        self.assertEqual(counts.get('solver_error'), 1)
        statuses = self.summary['status_counts_all_rows']
        self.assertEqual(statuses.get('Optimal'), 4)
        self.assertEqual(statuses.get('ParseError'), 1)
        self.assertEqual(statuses.get('ParentTimeout'), 1)
        self.assertEqual(self.summary['rows_emitted'], 7)

    def test_absent_cell_stays_in_denominator(self):
        solved = self.summary['solved_fraction']
        self.assertEqual(solved['denominator'], 5)
        self.assertEqual(solved['numerator'], 2)
        self.assertEqual(solved['value'], 2 / 5)
        self.assertEqual(self.summary['run_state_counts'].get('absent'), 1)

    def test_objective_disagreement_detected_at_t4(self):
        found = self.summary['objective_disagreement']
        self.assertEqual(found['tolerance'], COLD)
        self.assertEqual(found['numerator'], 1)
        self.assertEqual(found['denominator'], 1)
        self.assertEqual(found['disagreements'][0]['instance'], 'cell_a.mps')
        self.assertGreater(found['disagreements'][0]['spread'], COLD)

    def test_independent_failure_keeps_solver_status(self):
        failures = self.summary['independent_primal_failures']
        self.assertEqual(failures['numerator'], 1)
        self.assertEqual(failures['denominator'], 4)
        row = next(r for r in self.rows if r['instance'] == 'cell_b.mps')
        self.assertEqual(row['status'], 'Optimal')
        self.assertEqual(row['independent_check'], 'fail')
        self.assertEqual(row['verified'], 'true')

    def test_cold_and_warm_are_never_merged(self):
        counts = self.summary['cold_warm_counts']
        self.assertEqual(counts.get('cold'), 4)
        self.assertEqual(counts.get('warm'), 3)
        for key in self.summary['time_distribution']:
            self.assertIn(':', key)
            self.assertIn(key.split(':')[1], ('cold', 'warm'))

    def test_row_builder_never_rewrites_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            model = Path(tmp) / 'selftest.mps'
            model.write_text(TINY_MPS)
            entry = {'id': str(model), 'suite': 'fabricated',
                     'problem_class': 'LP', 'split': 'train', 'threads': 1,
                     'solver_path': 'fabricated', 'present': True,
                     'sha256': ''}
            passing = {'run_state': 'ok', 'status': 'Optimal',
                       'verified': True, 'assurance': 'optimality_witness_checked',
                       'objective': 1.0, 'primal': [1.0],
                       'variable_names': ['X1'], 'notes': ''}
            row = campaign._finish_row(entry, passing, 'selftest', 'selftest',
                                       '0' * 64, 0, 60.0, None)
            self.assertEqual(row['independent_check'], 'pass')
            self.assertEqual(row['status'], 'Optimal')
            violating = dict(passing, primal=[0.99999])
            row = campaign._finish_row(entry, violating, 'selftest', 'selftest',
                                       '0' * 64, 0, 60.0, None)
            self.assertEqual(row['independent_check'], 'fail')
            self.assertEqual(row['status'], 'Optimal')
            self.assertIn('solver_reported_verified', row['notes'])
            empty = dict(passing, primal=[])
            row = campaign._finish_row(entry, empty, 'selftest', 'selftest',
                                       '0' * 64, 0, 60.0, None)
            self.assertEqual(row['independent_check'], 'not_checked')

    def test_no_fabricated_record_reaches_evidence(self):
        evidence = ROOT / 'evidence'
        self.assertFalse(
            any(evidence.glob('*selftest*')),
            'a fabricated record leaked into evidence/')

    def test_every_row_state_is_declared(self):
        for row in self.rows:
            self.assertIn(row['run_state'], RUN_STATES)


if __name__ == '__main__':
    unittest.main(verbosity=2)
