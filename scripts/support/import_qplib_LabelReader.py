from __future__ import annotations
from .import_qplib_config import (
    pathlib
)

class Reader:
    """Line-granular reader for the positional QPLIB body.

    The format mixes numeric records with prose trailers on the same line
    (``0.0   default value for entries in initial x``), so records must be read
    *line by line*, taking only the leading tokens that are numeric.
    """

    def __init__(self, lines: list[str]):
        self.lines = lines
        self.pos = 0

    def fields(self) -> list[str]:
        while self.pos < len(self.lines):
            tokens = self.lines[self.pos].split()
            self.pos += 1
            if tokens:
                return tokens
        raise ValueError("unexpected end of QPLIB file")

    def integer(self) -> int:
        return int(float(self.fields()[0]))

    def number(self) -> float:
        return float(self.fields()[0])

    def pairs(self, count: int) -> dict[int, float]:
        out: dict[int, float] = {}
        for _ in range(count):
            tokens = self.fields()
            out[int(float(tokens[0]))] = float(tokens[1])
        return out

    def triples(self, count: int) -> list[tuple[int, int, float]]:
        out = []
        for _ in range(count):
            tokens = self.fields()
            out.append((int(float(tokens[0])), int(float(tokens[1])), float(tokens[2])))
        return out

def split_records(path: pathlib.Path) -> list[tuple[str, str]]:
    """Split a QPLIB file into ``(data, comment)`` records.

    Two dialects exist: QPLIB's own distribution annotates every positional
    record with its ``# ...`` label ("number of constraints", "default
    variable lower bound value", ...), while generated instances omit the
    comments entirely.  The labels are what makes optional records (constraint
    data, starting points, variable bounds) distinguishable from mandatory
    ones, so they are kept.
    """
    records: list[tuple[str, str]] = []
    for line in path.read_text(errors="replace").splitlines():
        data, _, comment = line.partition("#")
        data = data.strip()
        if data:
            records.append((data, comment.strip().lower()))
    return records

class LabelReader:
    """Cursor over an *annotated* QPLIB body (every record carries its label).

    ``section(label)`` reads a ``default`` + ``count`` section only when the
    next record's own comment identifies it, returning ``None`` for records
    the writer omitted (constraint data for an unconstrained instance,
    starting points, ...).  Skipping an unrecognised record is never done
    silently: the caller raises, so a dialect change surfaces as a per-instance
    import failure instead of a shifted parse.
    """

    def __init__(self, records: list[tuple[str, str]]):
        self.records = records
        self.pos = 0

    def comment(self) -> str:
        if self.pos >= len(self.records):
            return ""
        return self.records[self.pos][1]

    def record(self) -> str:
        if self.pos >= len(self.records):
            raise ValueError("unexpected end of QPLIB file")
        text = self.records[self.pos][0]
        self.pos += 1
        return text

    def fields(self) -> list[str]:
        tokens = self.record().split()
        if not tokens:
            raise ValueError("unexpected end of QPLIB file")
        return tokens

    def integer(self) -> int:
        return int(float(self.fields()[0]))

    def number(self) -> float:
        return float(self.fields()[0])

    def pairs(self, count: int) -> dict[int, float]:
        out: dict[int, float] = {}
        for _ in range(count):
            tokens = self.fields()
            out[int(float(tokens[0]))] = float(tokens[1])
        return out

    def triples(self, count: int) -> list[tuple[int, int, float]]:
        out = []
        for _ in range(count):
            tokens = self.fields()
            out.append((int(float(tokens[0])), int(float(tokens[1])), float(tokens[2])))
        return out

    def section(self, label: str) -> tuple[float, dict[int, float]] | None:
        if label not in self.comment():
            return None
        default = self.number()
        return default, self.pairs(self.integer())
