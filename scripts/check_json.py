#!/usr/bin/env python3
"""Validate tracked JSON without traversing build trees or local editor state."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
paths = subprocess.check_output(["git", "ls-files", "-z", "*.json"], cwd=root).decode().split("\0")
for relative in filter(None, paths):
    if ".obsidian" in Path(relative).parts:
        continue
    path = root / relative
    if path.exists():
        with path.open(encoding="utf-8") as stream:
            json.load(stream)
