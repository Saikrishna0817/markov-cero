# MPS rim input contract — objective-row RHS/RANGES (library, v1)

**Status:** binding from 2026-10-01. Applies to `io::parse_mps`,
`io::parse_mps_string` and therefore every CLI, binding and hosted path that
reads free-format MPS.

Scope: how the `RHS` and `RANGES` sections are interpreted when the entry names
an `N` (objective) row. This closes the two `InvalidModel` rejections the
BENCH-01 campaign surfaced (`data/netlib/e226.mps` line 1683,
`data/netlib/grow7.mps` line 1518) by giving the entry one documented meaning
instead of refusing it.

## 1. An objective-row RHS is the objective constant

- An `RHS` entry whose row is the objective row sets
  `model.objective_offset += value` — the free-MPS objective constant, so the
  minimized (or maximized) expression is `cᵀx + offset`.
- Every engine, verifier and reported objective already adds
  `objective_offset`; nothing else in the solve path changes. The offset is
  part of the model fingerprint (`model_hash`), so two files that differ only
  in this entry do not share a fingerprint.
- The **objective row** is decided before the `RHS` section is read: `OBJNAME`
  if present, otherwise the first `N` row in `ROWS`.
- The value must parse as a number and be finite, and the accumulated offset
  must stay finite; otherwise the record is `invalid_model`.
- A second `RHS` entry for the objective row is a `duplicate RHS row`
  rejection, exactly as for a constrained row. One objective constant, one
  record.

## 2. What stays rejected (fail closed, never dropped)

| Input | Outcome |
|---|---|
| `RANGES` entry naming the objective row | `MpsError`: a range narrows a constrained row's slack interval and has no meaning for an objective. No file in `data/` or `examples/` uses one; the reader rejects rather than silently discarding it. |
| `RHS`/`RANGES` entry naming a non-objective `N` row | `MpsError`. Only one `N` row can be optimized; a rim value on any other `N` row has no target. |
| Second `RHS` or `RANGES` entry for the same row | `MpsError` (`duplicate RHS row` / `duplicate RANGES row`), including the objective row. |
| More than one rim vector name in one section | `MpsError` (`multiple rim vectors are unsupported`), unchanged from v0. |

Anything not listed as accepted in §1 and not explicitly rejected in §2 is not
promised: this contract is about objective-row rim entries only.

## 3. Test obligations

`tests/mps_parser_test.cpp` (CTest `mps_parser`) must cover:

1. an objective-row `RHS` entry is accepted and lands in
   `model.objective_offset` with the file's value;
2. a duplicate objective-row `RHS` entry is rejected;
3. an objective-row `RANGES` entry is rejected;
4. a rim entry on a non-objective `N` row is rejected.

Both motivating netlib models must parse: `data/netlib/e226.mps`
(offset `-7.113`) and `data/netlib/grow7.mps` (offset `0`).

## 4. Not claimed

- No claim that either newly parsing model solves, nor any optimal value,
  status or performance claim for them. Parsing is not solving.
- No claim about `RANGES`/`RHS` semantics for constrained rows; those are
  unchanged from the reader's original behavior and covered by existing tests.
- No change to any tolerance, status, assurance label or bound.
