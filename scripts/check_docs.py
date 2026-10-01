#!/usr/bin/env python3
"""Check local links in current user-facing documentation; archived research is excluded."""
from pathlib import Path
import re
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
CURRENT = ['README.md', 'docs/README.md', 'docs/guides/BUILDING.md',
           'docs/guides/QUICKSTART.md', 'docs/guides/VERIFY.md',
           'docs/contracts/numerical-policy.md',
           'docs/contracts/resource-limits.md',
           'docs/contracts/sparse-lp-path.md',
           'docs/contracts/convex-qp.md',
           'docs/contracts/milp-node-bounds.md',
           'docs/contracts/miqp-node-bounds.md',
           'docs/contracts/nlp-local-sqp.md',
           'docs/contracts/nlp-restoration.md',
           'docs/contracts/minlp-oa.md',
           'docs/contracts/minlp-proof-replay.md',
           'docs/contracts/hosted-limits.md',
           'docs/project/STATUS.md', 'docs/project/PROVENANCE.md',
           'docs/project/ORIGINAL_REQUEST.md', 'docs/research/README.md',
           'CHANGELOG.md', 'web/README.md', 'examples/cases/README.md',
           'examples/refinery/README.md', 'examples/refinery/data-dictionary.md',
           'data/mittelmann/README.md', 'data/qp/README.md',
           'third_party/LICENSES/README.md', 'evidence/INDEX.md']
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
