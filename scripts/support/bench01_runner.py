"""BENCH-01 contract section 4: one matched solver process.

The parent owns the observation: wall clock from spawn to reap, a
watchdog at ``cap + parent_slack_s``, and the per-child peak RSS.

Peak RSS caveat: ``os.wait4``'s ``ru_maxrss`` reports
``max(parent_high_water, child_high_water)`` on this host — after a
``fork``/``vfork`` the child's high-water mark is inherited and an
``exec`` does not reset it — so a parent that has ever held more memory
than its children poisons every reading. The runner therefore samples
``/proc/<pid>/status`` ``VmHWM`` while the child runs, which is
per-mm and resets at exec. ``/usr/bin/time`` is not available here.
"""

from __future__ import annotations

import json
import os
import selectors
import signal
import subprocess
import time

KILL_GRACE_S = 5.0
READ_CHUNK = 65536
SAMPLE_INTERVAL_S = 0.002

RESULT_FIELDS = (
    'status', 'verified', 'assurance', 'objective', 'best_bound',
    'relative_gap', 'nodes_explored', 'lp_iterations', 'runtime_ms',
    'guarantee_tier', 'stop_reason',
)


def _kill_group(pid: int) -> None:
    for sig in (signal.SIGKILL,):
        try:
            os.killpg(pid, sig)
        except (ProcessLookupError, PermissionError, OSError):
            try:
                os.kill(pid, sig)
            except OSError:
                pass


def _peak_rss_kib(pid: int) -> int:
    """VmHWM of one process, in KiB; 0 when the entry is gone."""
    try:
        with open(f'/proc/{pid}/status', 'rb') as handle:
            for line in handle:
                if line.startswith(b'VmHWM:'):
                    return int(line.split()[1])
    except (OSError, ValueError, IndexError):
        pass
    return 0


def _drain(process: subprocess.Popen, deadline: float) -> tuple:
    """Read both pipes until EOF or deadline; return (out, err, timed_out, peak)."""
    out, err, peak = bytearray(), bytearray(), 0
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ, out)
    selector.register(process.stderr, selectors.EVENT_READ, err)
    timed_out = False
    try:
        while selector.get_map():
            peak = max(peak, _peak_rss_kib(process.pid))
            now = time.monotonic()
            if now >= deadline and not timed_out:
                timed_out = True
                _kill_group(process.pid)
                deadline = now + KILL_GRACE_S
            wait = min(SAMPLE_INTERVAL_S, max(0.0, deadline - time.monotonic()))
            for key, _ in selector.select(wait):
                try:
                    chunk = os.read(key.fileobj.fileno(), READ_CHUNK)
                except OSError:
                    chunk = b''
                if not chunk:
                    selector.unregister(key.fileobj)
                    continue
                key.data.extend(chunk)
            if timed_out and time.monotonic() > deadline:
                break
    finally:
        selector.close()
    return bytes(out), bytes(err), timed_out, peak


def _reap(process: subprocess.Popen) -> int:
    for name in ('stdout', 'stderr'):
        handle = getattr(process, name, None)
        if handle is not None and not handle.closed:
            try:
                handle.close()
            except OSError:
                pass
    pid, status, _usage = os.wait4(process.pid, 0)
    code = os.waitstatus_to_exitcode(status)
    process.returncode = code
    return code


def _decode(payload: bytes) -> tuple:
    """Return (parsed dict, note). Strict: stdout is JSON and nothing else."""
    text = payload.decode('utf-8', errors='replace').strip()
    if not text:
        return None, 'empty_stdout'
    try:
        data = json.loads(text)
    except ValueError as exc:
        tail = ' '.join(text.split())[:80]
        return None, f'unparseable_stdout:{type(exc).__name__}:{tail}'
    if not isinstance(data, dict) or 'status' not in data:
        return None, 'stdout_json_without_status'
    return data, ''


def run_once(binary: str, model_path: str, threads: int, cap_s: float,
             parent_slack_s: float, extra_args=()) -> dict:
    """Spawn one solver process and return its contract-section-5 record."""
    argv = [binary, str(model_path), '--threads', str(threads),
            '--time-limit', str(cap_s)]
    argv.extend(str(a) for a in extra_args)
    record = {field: '' for field in RESULT_FIELDS}
    record.update({
        'run_state': 'solver_error', 'parent_wall_ms': 0.0,
        'solver_runtime_ms': '', 'peak_rss_kib': '', 'primal': None,
        'variable_names': None, 'notes': '', 'exit_code': '',
        'stdout_bytes': 0, 'stderr_tail': '',
    })
    started = time.monotonic()
    try:
        process = subprocess.Popen(
            argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            stdin=subprocess.DEVNULL, start_new_session=True)
    except OSError as exc:
        record['notes'] = f'spawn_failed:{type(exc).__name__}:{exc}'
        return record
    deadline = started + float(cap_s) + float(parent_slack_s)
    payload, err, timed_out, peak_kib = _drain(process, deadline)
    try:
        exit_code = _reap(process)
    except ChildProcessError:
        exit_code = 0
    record['parent_wall_ms'] = round((time.monotonic() - started) * 1000.0, 3)
    record['peak_rss_kib'] = peak_kib
    record['exit_code'] = exit_code
    record['stdout_bytes'] = len(payload)
    record['stderr_tail'] = ' '.join(err.decode('utf-8',
                                                 errors='replace').split())[:200]
    if timed_out:
        record['run_state'] = 'parent_timeout'
        record['status'] = 'ParentTimeout'
        record['notes'] = 'parent_watchdog'
        return record
    data, note = _decode(payload)
    if data is None:
        record['notes'] = note or 'no_solver_result'
        return record
    for field in RESULT_FIELDS:
        if field in data:
            record[field] = data[field]
    record['run_state'] = 'ok'
    record['solver_runtime_ms'] = data.get('runtime_ms', '')
    record['primal'] = data.get('primal')
    record['variable_names'] = data.get('variable_names')
    notes = []
    if exit_code != 0:
        notes.append(f'exit_code={exit_code}')
    if record['stderr_tail']:
        notes.append(f'stderr={record["stderr_tail"]}')
    record['notes'] = '; '.join(notes)
    return record
