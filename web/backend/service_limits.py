"""REL-01 service controls: request quota, counters and slot accounting.

Contract: docs/contracts/hosted-limits.md v1.1 sections 4 and 5. Every
counter here is process-lifetime monotonic and the snapshot carries no model
text, objective, filename or client identity.
"""

from __future__ import annotations

import math
import os
import threading
import time

# REL-01 (hosted-limits v1.1 §4): per-client fixed-window solve quota. The
# window and the budget are read once at import time, like the OS limits.
REQUEST_QUOTA_MAX_REQUESTS = int(os.environ.get("REQUEST_QUOTA_MAX_REQUESTS", "60"))
REQUEST_QUOTA_WINDOW_SECONDS = float(os.environ.get("REQUEST_QUOTA_WINDOW_SECONDS", "60"))


class Quota:
    """Fixed-window counter with a bounded number of tracked keys."""

    def __init__(self, limit, window):
        self.limit = limit
        self.window = window
        self._lock = threading.Lock()
        self._seen = {}

    def spend(self, key):
        """Charge one request. Returns (allowed, retry_after_seconds)."""
        now = time.monotonic()
        with self._lock:
            if len(self._seen) >= 4096:
                self._seen = {k: v for k, v in self._seen.items()
                              if now - v[0] < self.window}
            slot = self._seen.get(key)
            if slot is None or now - slot[0] >= self.window:
                self._seen[key] = [now, 1]
                return True, 0
            if slot[1] >= self.limit:
                return False, max(1, math.ceil(self.window - (now - slot[0])))
            slot[1] += 1
            return True, 0


QUOTA = Quota(REQUEST_QUOTA_MAX_REQUESTS, REQUEST_QUOTA_WINDOW_SECONDS)

# Process-lifetime counters, monotonic, no history (hosted-limits v1.1 §4).
_METRICS_LOCK = threading.Lock()
_METRICS = {
    "requests_total": 0, "solves_total": 0, "solves_accepted": 0,
    "quota_rejected": 0, "busy_rejected": 0, "auth_rejected": 0,
    "client_error": 0, "server_error": 0, "os_limit_kills": 0,
    "solver_failures": 0, "timeouts": 0,
}
_SOLVE_STATUS = {}
SLOTS_TOTAL = 2
SLOTS_USED = 0


def bump(name, n=1):
    with _METRICS_LOCK:
        _METRICS[name] = _METRICS.get(name, 0) + n


def record(status, path):
    with _METRICS_LOCK:
        _METRICS["requests_total"] += 1
        if status == 200 and path == "/solve":
            _METRICS["solves_accepted"] += 1
        elif status == 401:
            _METRICS["auth_rejected"] += 1
        elif status == 429:
            pass  # quota_rejected / busy_rejected are charged at the branch
        elif status == 422:
            pass  # os_limit_kills / solver_failures are charged at the branch
        elif status == 504:
            pass  # timeouts is charged at the branch
        elif 400 <= status < 500:
            _METRICS["client_error"] += 1
        elif 500 <= status < 600:
            _METRICS["server_error"] += 1


def record_status(status):
    with _METRICS_LOCK:
        _SOLVE_STATUS[status] = _SOLVE_STATUS.get(status, 0) + 1


def metrics_snapshot(solver_ready, slots_in_use):
    with _METRICS_LOCK:
        payload = dict(_METRICS)
        payload["solves_by_status"] = dict(_SOLVE_STATUS)
    payload["solver_ready"] = solver_ready
    payload["slots_in_use"] = slots_in_use
    payload["slots_total"] = SLOTS_TOTAL
    return payload


def slots_acquire(semaphore):
    global SLOTS_USED
    if not semaphore.acquire(blocking=False):
        return False
    with _METRICS_LOCK:
        SLOTS_USED += 1
    return True


def slots_release(semaphore):
    global SLOTS_USED
    with _METRICS_LOCK:
        SLOTS_USED = max(0, SLOTS_USED - 1)
    semaphore.release()


def slots_used():
    with _METRICS_LOCK:
        return SLOTS_USED

