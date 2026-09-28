#!/usr/bin/env python3
"""Generate backlinks for the Obsidian vault under docs/.

Walks docs/**/*.md, parses [[stem]] / [[stem|alias]] wikilinks (ignoring fenced
code blocks and inline code spans), builds a reverse index of target -> sources,
and rewrites the auto-populated body of each "## Referenced By" section.

Stdlib only. Idempotent: links inside generated "## Referenced By" sections are
excluded from the index, and generated sections are re-written on every run.

Usage:  python3 scripts/link_backlinks.py [--check] [--quiet]
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

DOCS = Path(__file__).resolve().parent.parent / "docs"

WIKILINK_RE = re.compile(r"\[\[([^\[\]\n]+?)\]\]")
FENCE_RE = re.compile(r"^\s*(`{3,}|~{3,})")
INLINE_CODE_RE = re.compile(r"`[^`\n]*`")
HEADING2_RE = re.compile(r"^##\s+\S")
RB_HEADING_RE = re.compile(r"^##\s+Referenced By\s*$")
ITALIC_LINE_RE = re.compile(r"^\s*_.+_\s*$")
GENERATED_BULLET_RE = re.compile(r"^\s*-\s*\[\[[^\[\]]+\]\]\s*$")
BACKLINK_BULLET_RE = "- [[{stem}|{path}]]"


def mask_inline_code(line: str) -> str:
    """Blank out `inline code` spans so wikilinks inside them are not parsed."""
    return INLINE_CODE_RE.sub(lambda m: " " * len(m.group(0)), line)


def mask_code(text: str) -> str:
    """Return `text` with fenced blocks and inline code blanked out.

    Newlines are preserved so line numbers stay aligned with the original.
    """
    out: list[str] = []
    fence: str | None = None
    for line in text.split("\n"):
        m = FENCE_RE.match(line)
        if m:
            token = m.group(1)[0]
            if fence is None:
                fence = token
                out.append("")
                continue
            if token == fence:
                fence = None
                out.append("")
                continue
        if fence is not None:
            out.append("")
            continue
        out.append(mask_inline_code(line))
    return "\n".join(out)


def split_sections(lines: list[str]) -> list[tuple[str, int, int]]:
    """Split into (heading_text, start, end) blocks for every ## heading.

    start points at the heading line, end is exclusive. A leading block with no
    heading is reported as heading "".
    """
    heads: list[tuple[str, int]] = []
    for i, line in enumerate(lines):
        if HEADING2_RE.match(line):
            heads.append((line[2:].strip(), i))
    if not heads:
        return [("", 0, len(lines))]
    sections: list[tuple[str, int, int]] = []
    if heads[0][1] > 0:
        sections.append(("", 0, heads[0][1]))
    for idx, (title, start) in enumerate(heads):
        end = heads[idx + 1][1] if idx + 1 < len(heads) else len(lines)
        sections.append((title, start, end))
    return sections


def strip_referenced_by(masked: str) -> str:
    """Remove generated '## Referenced By' bodies so they never feed the index."""
    lines = masked.split("\n")
    keep: list[str] = []
    skipping = False
    for line in lines:
        if RB_HEADING_RE.match(line):
            skipping = True
            keep.append(line)
            continue
        if skipping and HEADING2_RE.match(line):
            skipping = False
        if not skipping:
            keep.append(line)
    return "\n".join(keep)


def extract_links(masked: str) -> list[str]:
    """Raw wikilink targets (target or target|alias), code and backlink sections removed."""
    return [m.group(1).strip() for m in WIKILINK_RE.finditer(strip_referenced_by(masked))]


def resolve_path_target(target: str, docs: Path) -> Path | None:
    """Resolve a link that contains '/' against the vault (and repo root)."""
    raw = target[:-3] if target.endswith(".md") else target
    candidates = [
        docs / f"{raw}.md",
        docs / raw,
        docs.parent / f"{raw}.md",
        docs.parent / raw,
    ]
    for cand in candidates:
        if cand.is_file():
            return cand
    return None


def italic_placeholder(lines: list[str], start: int, end: int) -> bool:
    """True when the first non-blank line after the heading is an italic placeholder."""
    for line in lines[start + 1 : end]:
        if not line.strip():
            continue
        return bool(ITALIC_LINE_RE.match(line))
    return False


def generated_section(lines: list[str], start: int, end: int) -> bool:
    """True when the body is empty or consists solely of '- [[...]]' bullets."""
    body = [l for l in lines[start + 1 : end] if l.strip()]
    return all(GENERATED_BULLET_RE.match(l) for l in body)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true", help="report only, never write")
    ap.add_argument("--quiet", action="store_true", help="suppress per-note output")
    args = ap.parse_args()

    if not DOCS.is_dir():
        print(f"error: vault not found at {DOCS}", file=sys.stderr)
        return 2

    files = sorted(p for p in DOCS.rglob("*.md") if p.is_file())

    # ---- basename (stem) index across the whole vault -------------------
    stem_index: dict[str, list[Path]] = defaultdict(list)
    for p in files:
        stem_index[p.stem].append(p)

    rel = {p: p.relative_to(DOCS).as_posix() for p in files}
    masked: dict[Path, str] = {}
    for p in files:
        try:
            masked[p] = mask_code(p.read_text(encoding="utf-8", errors="replace"))
        except OSError as exc:
            print(f"warn: cannot read {p}: {exc}", file=sys.stderr)
            masked[p] = ""

    # ---- link extraction + resolution -----------------------------------
    links_total = 0
    ignored_slash = 0
    resolved_links = 0
    ambiguous_links = 0
    unresolved_links = 0
    unresolved: dict[str, set[str]] = defaultdict(set)   # stem -> source rel paths
    ambiguous: dict[str, list[str]] = {}                 # stem -> candidate rel paths
    backlinks: dict[str, set[str]] = defaultdict(set)    # target rel -> source rel
    known_rels = set(rel.values())

    for src, text in masked.items():
        src_rel = rel[src]
        for raw in extract_links(text):
            target = raw.split("|", 1)[0].strip()
            alias_free = target.split("#", 1)[0].strip()
            if not alias_free:
                continue
            links_total += 1
            if "/" in alias_free:
                hit = resolve_path_target(alias_free, DOCS)
                if hit is None or not hit.is_relative_to(DOCS):
                    ignored_slash += 1
                    continue
                tgt_rel = hit.relative_to(DOCS).as_posix()
                if tgt_rel not in known_rels:
                    ignored_slash += 1
                    continue
                resolved_links += 1
                backlinks[tgt_rel].add(src_rel)
                continue
            cands = stem_index.get(alias_free, [])
            if not cands:
                unresolved_links += 1
                unresolved[alias_free].add(src_rel)
            elif len(cands) > 1:
                ambiguous_links += 1
                ambiguous[alias_free] = sorted(
                    c.relative_to(DOCS).as_posix() for c in cands
                )
            else:
                resolved_links += 1
                backlinks[rel[cands[0]]].add(src_rel)

    # ---- patch '## Referenced By' sections --------------------------------
    patched: list[str] = []
    empty_sections: list[str] = []
    skipped_ambiguous_section: list[str] = []
    writes: list[tuple[Path, str]] = []

    for p in files:
        text = p.read_text(encoding="utf-8", errors="replace")
        lines = text.split("\n")
        masked_lines = masked[p].split("\n")
        if len(masked_lines) != len(lines):
            masked_lines = lines
        for title, start, end in split_sections(masked_lines):
            if title != "Referenced By" or not RB_HEADING_RE.match(lines[start]):
                continue
            if not (italic_placeholder(masked_lines, start, end)
                    or generated_section(masked_lines, start, end)):
                continue
            stem = p.stem
            if stem in ambiguous:
                skipped_ambiguous_section.append(rel[p])
                continue
            srcs = sorted(backlinks.get(rel[p], set()) - {rel[p]})
            if not srcs:
                empty_sections.append(rel[p])
                continue
            body = [""]
            for s in srcs:
                body.append(BACKLINK_BULLET_RE.format(stem=Path(s).stem, path=s[:-3]))
            new_lines = lines[: start + 1] + body + lines[end:]
            new_text = "\n".join(new_lines)
            if new_text != text:
                patched.append(rel[p])
                if not args.check:
                    writes.append((p, new_text))
            text = new_text
            lines = new_lines
            break

    if not args.check:
        for p, new_text in writes:
            p.write_text(new_text, encoding="utf-8")

    # ---- summary ----------------------------------------------------------
    unresolved_count = sum(len(v) for v in unresolved.values())
    ambiguous_stems = sorted(ambiguous)

    print("=" * 64)
    print("link_backlinks.py summary")
    print("=" * 64)
    print(f"notes scanned        : {len(files)}")
    print(f"links total          : {links_total}")
    print(f"  resolved           : {resolved_links}")
    print(f"  ignored (slash, no path): {ignored_slash}")
    print(f"  unresolved links   : {unresolved_links}  ({unresolved_count} note->stem refs)")
    print(f"  unresolved stems   : {len(unresolved)}")
    print(f"  ambiguous links    : {ambiguous_links}")
    print(f"  ambiguous stems    : {len(ambiguous_stems)}")
    print(f"notes patched        : {len(patched)}" + ("  (dry-run)" if args.check else ""))
    if empty_sections:
        print(f"sections with 0 backlinks (left as-is): {len(empty_sections)}")
    if skipped_ambiguous_section:
        print(f"sections skipped (ambiguous target): {len(skipped_ambiguous_section)}")

    if ambiguous_stems:
        print("\nAmbiguous stems (basename in 2+ folders, NOT written):")
        for s in ambiguous_stems:
            print(f"  {s}: {', '.join(ambiguous[s])}")

    if unresolved:
        print("\nUnresolved stems (referenced, no file exists):")
        for s in sorted(unresolved, key=lambda k: (-len(unresolved[k]), k)):
            print(f"  {s}  <- {len(unresolved[s])} note(s)")

    if not args.quiet and patched:
        print("\nPatched notes:")
        for s in sorted(patched):
            print(f"  {s}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
