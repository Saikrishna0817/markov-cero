# The NLOBJ MPS Section — W1 Path B Specification (D-12, D-20)

**Status:** LOCKED format (implementation plan v1.0, D-12/D-20)
**Scope:** file-based NLP / MINLP input as a **non-standard MPS extension**. Standard MPS
files are unaffected; a file without `NLOBJ` parses exactly as before.

## 1. Motivation

AMPL `.nl` was rejected (D-12) because a full parser for an external language specification
is out of scope without a third-party library. `NLOBJ` extends the MPS section list with a
minimal, self-describing block that covers:

- polynomial objective terms up to degree 2 (beyond the linear `OBJECTO`/`COLUMNS` rows), and
- the classification signal that a model is NLP or MINLP rather than LP/MILP/QP/MIQP.

Transcendental functions (`sin`, `exp`, ...) are **not expressible** in `NLOBJ`; they require
the callback API (D-01 Path A). The solver reports this limitation when an NLOBJ-only model
is used for such problems.

## 2. Grammar

The section appears between any standard section and `ENDATA`:

```
NLOBJ
* comment lines start with '*' in column 1
  <coeff>  <var0> [<var1>]          <- objective monomial
NLCON
  <coeff>  <var0> [<var1>]  <= <rhs>  <- nonlinear inequality g(x) <= rhs
ENDATA
```

Fields are whitespace-separated. `NLCON` is optional; `NLOBJ` may contain objective terms only.

### Objective monomials (`NLOBJ` body)

| Record | Meaning |
|---|---|
| `c  v0`        | linear term `c * x(v0)` (duplicate of COLUMNS entries is allowed and additive) |
| `c  v0  v1`    | degree-2 term `c * x(v0) * x(v1)`; if `v0 == v1` this is the diagonal `c * x(v0)^2` |

Variables are referenced by their MPS column (variable) **names**. A name that does not
appear in `COLUMNS` is a parse error.

### Nonlinear constraints (`NLCON` body)

Each record contributes one inequality

```
c * x(v0) [* x(v1)] + (rest of record) <= rhs
```

The current locked format supports **one monomial per NLCON record** plus its constant `rhs`.
A constraint needing several monomials is expressed as several records; the parser merges
records that share the same constraint name (see §3).

## 3. Named constraint merging

`NLCON` records may carry a trailing name token (a fourth/fifth field). Records with the same
name accumulate into a single constraint whose total coefficient is the sum. Records without
a name get an auto-generated name `nlcon_1`, `nlcon_2`, ... in file order.

## 4. Classification interaction (W6)

Non-empty `NLOBJ` or `NLCON` content sets `Model::has_nlobj_section = true`, allowing
constraint-only nonlinear models to reach the NLP/MINLP solver path. `NLOBJ` content maps
through the W6 classifier as follows:

| `NLOBJ` present | integer variables? | class |
|---|---|---|
| yes | no  | `nlp`  |
| yes | yes | `minlp` |

A model with only `QUADOBJ` (no `NLOBJ`) remains `qp`/`miqp` — never reclassified.

## 5. Semantics inside the solver

- The linear objective from `COLUMNS` plus the `NLOBJ` monomials together form the objective
  `f(x) = offset + c_lin^T x + sum_k c_k * prod(...)`. Degree-1 `NLOBJ` entries duplicate the
  linear part **additively**.
- The SQP engine (D-02) evaluates `f` and `grad f` from these terms; no callbacks are needed.
- `NLCON` constraints become nonlinear inequality callbacks `g_i(x) = monomial(x) - rhs <= 0`
  consumed by the SQP/outer-approximation machinery like any Path A constraint.

## 6. Example

```
NAME          POLY2
ROWS
 N  obj
 G  c1
COLUMNS
    x1        obj       1.0     c1        -1.0
    x2        obj       1.0
RHS
    rhs       c1        1.5
BOUNDS
 UP bnd       x1        3.0
 UP bnd       x2        3.0
NLOBJ
* 0.5 x1^2 + 0.5 x2^2  (split for readability)
  0.5   x1  x1
  0.5   x2  x2
NLCON
* nonlinear side constraint: 0.25 * x1^2 <= 0.5
  0.25  x1  x1    <= 0.5    nl1
ENDATA
```

The objective is `x1 + x2 + 0.5 x1^2 + 0.5 x2^2` with `x1 + x2 >= 1.5`,
`0.25 x1^2 <= 0.5`, `0 <= x1, x2 <= 3`.

## 7. Parser behavior and error handling

- `NLOBJ` / `NLCON` section headers are matched case-insensitively (like all MPS headers).
- Unknown variable name, malformed number, or degree > 2 → `ParseError` with line number
  (zero silent failures, C3).
- Empty `NLOBJ` section (header only) is legal but does **not** flip classification.
- The section is accepted in fixed and free MPS parsing modes.

## 8. Provenance

- Decision: D-12 (no AMPL `.nl`), D-20 (document here, treated as non-standard extension).
- Implementation: `src/io/nlobj_parser.cpp`, wired in `src/io/mps.cpp`.
- Tests: `tests/nlobj_parser_test.cpp`.
