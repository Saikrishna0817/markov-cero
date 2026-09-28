#!/usr/bin/env python3
"""Enforce 300 physical lines for maintained code and build files, including blanks."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = {'.cpp', '.hpp', '.c', '.h', '.cu', '.cuh', '.py', '.sh', '.cmake'}
DIRECTORIES = {'src', 'include', 'apps', 'tests', 'gpu', 'python', 'scripts', 'cmake', 'examples'}


def source_files():
    names = subprocess.check_output(
        ['git', 'ls-files', '-c', '-o', '--exclude-standard', '-z'], cwd=ROOT).split(b'\0')
    for raw in sorted(set(names)):
        if not raw:
            continue
        relative = Path(raw.decode())
        if (len(relative.parts) == 1 or relative.parts[0] in DIRECTORIES) and (
                relative.suffix in EXTENSIONS or relative.name == 'CMakeLists.txt'):
            path = ROOT / relative
            if path.is_file():
                yield relative, path


def main():
    failures = []
    count = 0
    maximum = 0
    for relative, path in source_files():
        lines = len(path.read_bytes().splitlines())
        count += 1
        maximum = max(maximum, lines)
        if lines > 300:
            failures.append(f'{relative}: {lines} physical lines (limit 300)')
    print(f'Checked {count} maintained files; maximum {maximum} lines; {len(failures)} violations')
    print('\n'.join(failures), end='\n' if failures else '')
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
