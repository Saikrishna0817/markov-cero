"""BENCH-01 contract §2/§7/§9 (docs/contracts/benchmark-campaign.md v1).

Frozen vocabulary shared by the campaign driver, the row runner, the
summariser and the harness self-test. Nothing here reads a result.
"""
from pathlib import Path as _EntryPath

_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] /
                   'scripts/run_bench01_campaign.py')

SCHEMA_VERSION = 'bench01-1'
MANIFEST_PATH = 'evidence/frozen-instances-20260928.json'

# Contract section 4: matched processes, frozen repeat structure.
DEFAULT_THREADS = 1
DEFAULT_REPEATS = 5
COLD_REPEAT_INDEX = 0
DEFAULT_WORKERS = 4
PARENT_SLACK_S = 30.0
DEFAULT_CAPS = {'LP': 60.0, 'QP': 60.0, 'MILP': 300.0}
SPLIT_ORDER = {'holdout': 0, 'tune': 1, 'train': 2}

# Contract section 5: every declared cell keeps exactly one row per repeat.
RUN_STATES = ('ok', 'solver_error', 'parent_timeout', 'absent',
              'hash_mismatch', 'thread_mismatch', 'binary_mutated')
INDEPENDENT_STATES = ('pass', 'fail', 'not_checked')

# Contract section 7: tolerances, locked.
TOLERANCES = {
    'objective_reference': {'LP': 1e-5, 'QP': 1e-4, 'MILP': 1e-4},
    'objective_repeat': 1e-6,
    'row_feasibility': 1e-6,
    'bound_feasibility': 1e-6,
    'integrality': 1e-6,
}

# Contract section 9: fixed column order for the raw CSV.
COLUMNS = (
    'campaign_id', 'manifest_version', 'solver', 'solver_sha256', 'instance',
    'suite', 'problem_class', 'split', 'repeat', 'cold_warm', 'run_state',
    'threads', 'cap_s', 'parent_wall_ms', 'solver_runtime_ms', 'status',
    'verified', 'assurance', 'objective', 'best_bound', 'relative_gap',
    'nodes', 'lp_iterations', 'peak_rss_kib', 'reference_objective',
    'objective_delta', 'independent_check', 'independent_max_violation',
    'notes',
)

# Contract section 10: what the self-test must prove with fabricated rows.
SELF_TEST_OBLIGATIONS = (
    'timeout_and_parse_error_stay_in_denominator',
    'objective_disagreement_detected_at_t4',
    'independent_check_failure_reported_without_status_rewrite',
    'absent_cell_stays_in_denominator',
    'cold_and_warm_reported_separately',
    'solved_fraction_uses_declared_denominator',
)

TIMING_SCOPE = ('parent-observed wall clock around the solver process, '
                'including model parse, CLI start-up and result serialisation')
