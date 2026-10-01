"""BENCH-01 contract section 8: independent primal check.

Parses the original model file with this repository's own reader and
recomputes row activities, bound residuals and integrality from the
returned primal vector. The verdict never reads the solver's
`row_activities`, `maximum_primal_violation` or `verified` field.
"""

INF = float('inf')
HEADERS = {'ROWS', 'COLUMNS', 'RHS', 'RANGES', 'BOUNDS', 'ENDATA',
           'OBJSENSE', 'QUADOBJ', 'QMATRIX', 'QMATRIX2', 'HMATRIX',
           'SOS', 'INDICATORS'}
QUADRATIC = {'QUADOBJ', 'QMATRIX', 'QMATRIX2', 'HMATRIX'}
NESTED = {'SOS', 'INDICATORS'}


class Unsupported(Exception):
    """A section or token this reader does not implement."""


class Model:
    def __init__(self):
        self.row_sense = {}   # name -> 'N' | 'L' | 'G' | 'E'
        self.row_rhs = {}     # name -> float
        self.row_range = {}   # name -> float (0 when absent)
        self.coef = {}        # row -> {var: value}
        self.lb = {}          # var -> float
        self.ub = {}          # var -> float
        self.integer = set()
        self.order = []       # variable order as first seen
        self.row_order = []
        self.notes = []


def _number(token: str) -> float:
    return float(token.replace('D', 'E').replace('d', 'e'))


def _isnum(token: str) -> bool:
    try:
        _number(token)
        return True
    except ValueError:
        return False


def _bounds(model: Model, var: str) -> None:
    if var not in model.lb:
        model.lb[var] = 0.0
        model.ub[var] = INF


def _apply_bound(model: Model, btype: str, var: str, value: float) -> None:
    if btype == 'LO':
        model.lb[var] = value
    elif btype == 'UP':
        model.ub[var] = value
    elif btype == 'FX':
        model.lb[var] = model.ub[var] = value
    elif btype == 'FR':
        model.lb[var], model.ub[var] = -INF, INF
    elif btype == 'MI':
        model.lb[var] = -INF
    elif btype == 'PL':
        model.ub[var] = INF
    elif btype in ('BV', 'LI', 'UI'):
        model.integer.add(var)
        if btype == 'BV':
            model.lb[var], model.ub[var] = 0.0, 1.0
        elif btype == 'LI':
            model.lb[var] = value


def parse(path) -> Model:
    model = Model()
    section = None
    in_integer_block = False
    seen_name = False
    for raw in path.read_text(errors='replace').splitlines():
        if not raw.strip() or raw.lstrip().startswith('*'):
            continue
        tokens = raw.split()
        head = tokens[0]
        if not seen_name and head == 'NAME':
            seen_name = True
            continue
        if len(tokens) == 1 and head in HEADERS:
            if head in NESTED:
                raise Unsupported(f'section {head} is not implemented')
            if head == 'ENDATA':
                section = None
                break
            section = head
            continue
        if section is None:
            continue
        if section == 'OBJSENSE':
            section = None
            continue
        if section in QUADRATIC:
            # Quadratic objective terms affect the objective only, never row
            # feasibility, so they are accepted and ignored here. This reader
            # never re-evaluates an objective (contract section 7, T1-T4).
            continue
        if section == 'ROWS':
            if len(tokens) != 2 or tokens[0] not in 'NLGE':
                raise Unsupported(f'bad ROWS line: {raw.strip()!r}')
            sense, name = tokens
            model.row_sense[name] = sense
            model.row_rhs[name] = 0.0
            model.row_range[name] = 0.0
            if name not in model.row_order:
                model.row_order.append(name)
            continue
        if section == 'COLUMNS':
            # Marker lines are three tokens with 'MARKER' in the middle; the
            # column name itself varies (MARKER000, MARK0000, ...).
            if len(tokens) == 3 and tokens[1].strip("'") == 'MARKER' and \
                    tokens[2].strip("'") in ('INTORG', 'INTEND'):
                in_integer_block = tokens[2].strip("'") == 'INTORG'
                continue
            if len(tokens) < 3 or (len(tokens) - 1) % 2:
                raise Unsupported(f'bad COLUMNS line: {raw.strip()!r}')
            var = tokens[0]
            _bounds(model, var)
            if var not in model.order:
                model.order.append(var)
            if in_integer_block:
                model.integer.add(var)
            for i in range(1, len(tokens), 2):
                row, value = tokens[i], _number(tokens[i + 1])
                if row not in model.row_sense:
                    raise Unsupported(f'row {row!r} used before ROWS')
                # Repeated (row, column) entries accumulate, as the MPS
                # standard requires; capitanescu_dc_opf relies on this.
                column = model.coef.setdefault(row, {})
                column[var] = column.get(var, 0.0) + value
            continue
        if section in ('RHS', 'RANGES'):
            target = model.row_rhs if section == 'RHS' else model.row_range
            # Odd token count: the leading vector name is present. Even:
            # the field was left blank and pairs start at token 0.
            start = 1 if len(tokens) % 2 else 0
            if (len(tokens) - start) % 2:
                raise Unsupported(f'bad {section} line: {raw.strip()!r}')
            for i in range(start, len(tokens), 2):
                row, value = tokens[i], _number(tokens[i + 1])
                if row not in target:
                    raise Unsupported(f'{section} row {row!r} unknown')
                target[row] = value
            continue
        if section == 'BOUNDS':
            if len(tokens) < 3:
                raise Unsupported(f'bad BOUNDS line: {raw.strip()!r}')
            btype, var = tokens[0], tokens[2]
            _bounds(model, var)
            if var not in model.order:
                model.order.append(var)
            value = _number(tokens[3]) if len(tokens) > 3 else 0.0
            if btype in ('LO', 'UP', 'FX', 'FR', 'MI', 'PL', 'BV', 'LI', 'UI'):
                _apply_bound(model, btype, var, value)
            else:
                raise Unsupported(f'unknown bound type {btype!r}')
            continue
        raise Unsupported(f'unhandled line in {section}: {raw.strip()!r}')
    return model


def _row_violation(sense: str, activity: float, rhs: float, rng: float) -> float:
    if sense == 'L':
        return activity - rhs
    if sense == 'G':
        return rhs - activity
    if not rng:
        return abs(activity - rhs)
    lo, hi = (rhs, rhs + rng) if rng >= 0 else (rhs + rng, rhs)
    return max(lo - activity, activity - hi)


def check(model: Model, primal, names, tol_row: float, tol_bound: float,
          tol_int: float) -> tuple:
    """Return (verdict, maximum_violation, note)."""
    if not model.row_order or not model.order:
        return 'not_checked', 0.0, 'model parsed to nothing'
    if primal is None or len(primal) == 0:
        return 'not_checked', 0.0, 'no primal vector in result'
    if names:
        if len(names) != len(primal):
            return 'not_checked', 0.0, 'primal and variable_names differ in length'
        if set(names) != set(model.order):
            return 'not_checked', 0.0, 'variable names disagree with model'
        value = dict(zip(names, primal))
    else:
        if len(primal) != len(model.order):
            return 'not_checked', 0.0, 'primal length disagrees with model'
        value = dict(zip(model.order, primal))
    worst = 0.0
    for row, sense in model.row_sense.items():
        if sense == 'N':
            continue
        activity = 0.0
        for var, coef in model.coef.get(row, {}).items():
            activity += coef * value.get(var, 0.0)
        viol = _row_violation(sense, activity, model.row_rhs[row],
                              model.row_range[row])
        if viol > worst:
            worst = viol
        if viol > tol_row:
            return 'fail', viol, f'row {row} exceeds feasibility by {viol:.3e}'
    for var in model.order:
        v = value.get(var, 0.0)
        lb, ub = model.lb.get(var, 0.0), model.ub.get(var, INF)
        viol = max(lb - v, v - ub, 0.0)
        if viol > worst:
            worst = viol
        if viol > tol_bound:
            return 'fail', viol, f'variable {var} bound exceeded by {viol:.3e}'
        if var in model.integer:
            integ = abs(v - round(v))
            if integ > worst:
                worst = integ
            if integ > tol_int:
                return 'fail', integ, f'variable {var} not integral by {integ:.3e}'
    return ('pass', worst, '') if worst <= max(tol_row, tol_bound) else \
        ('fail', worst, f'maximum violation {worst:.3e}')
