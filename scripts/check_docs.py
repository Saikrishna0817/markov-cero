#!/usr/bin/env python3
"""Check local links in current user-facing documentation; archived research is excluded."""
from pathlib import Path
import re
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
CURRENT = ['README.md', 'BUILDING.md', 'QUICKSTART.md', 'VERIFY.md', 'STATUS.md',
           'PROVENANCE.md', 'docs/governance/provenance-and-verification.md',
           'docs/audit/INDUSTRY-READINESS-IMPLEMENTATION-PLAN.md']
failures = []
for name in CURRENT:
    path = ROOT / name
    text = re.sub(r'```.*?```', '', path.read_text(), flags=re.S)
    for match in re.finditer(r'\[[^\]]*\]\(([^\s)]+)(?:\s+[^)]*)?\)', text):
        target = unquote(match.group(1).strip('<>').split('#')[0])
        if not target or re.match(r'^[A-Za-z][A-Za-z0-9+.-]*:', target):
            continue
        if not (path.parent / target).exists():
            failures.append(f'{name}: missing {target}')
for failure in failures:
    print(failure)
print(f'{len(CURRENT)} active documents checked; {len(failures)} broken local links')
sys.exit(bool(failures))
