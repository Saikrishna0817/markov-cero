#!/usr/bin/env python3
"""Generate the historical public Fawley LP from attributed numeric input data.

This reproduces an illustrative benchmark, not an operational refinery model.
Fuel-equivalent accounting and empirical blend proxies retain source semantics.
"""
import argparse
import hashlib
import json
from pathlib import Path


class LinearModel:
    def __init__(self):
        self.rows = {}
        self.columns = {}
        self.bounds = {}

    def row(self, name, sense, rhs=0.0):
        self.rows[name] = (sense, rhs)

    def variable(self, name, cost=0.0, lower=0.0, upper=None):
        self.columns[name] = {'OBJ': cost}
        self.bounds[name] = (lower, upper)

    def add(self, variable, row, coefficient):
        column = self.columns[variable]
        column[row] = column.get(row, 0.0) + coefficient

    def write(self, output, digest, flow_unit, objective_unit, quality_units):
        lines = ['* Historical public Fawley benchmark; no plant approval',
                 '* Input SHA256 ' + digest,
                 '* Units: flow ' + flow_unit + '; objective ' + objective_unit,
                 '* Quality units: ' + '; '.join(f'{name}={unit}' for name, unit in sorted(quality_units.items())),
                 '* Public illustrative qualification data; not plant operating data.',
                 'NAME FAWLEY_PUBLIC', 'ROWS', ' N OBJ']
        lines += [f' {sense} {name}' for name, (sense, _) in self.rows.items()]
        lines.append('COLUMNS')
        for name, entries in self.columns.items():
            lines += [f' {name} {row} {value:.17g}' for row, value in entries.items() if value]
            if not any(entries.values()):
                lines.append(f' {name} OBJ 0')
        lines.append('RHS')
        lines += [f' R {name} {rhs:.17g}' for name, (_, rhs) in self.rows.items() if rhs]
        lines.append('BOUNDS')
        for name, (lower, upper) in self.bounds.items():
            if upper is not None and lower == upper:
                lines.append(f' FX B {name} {lower:.17g}')
            else:
                if lower:
                    lines.append(f' LO B {name} {lower:.17g}')
                if upper is not None:
                    lines.append(f' UP B {name} {upper:.17g}')
        lines.append('ENDATA')
        output.write_text('\n'.join(lines) + '\n')


def build(data):
    model = LinearModel()
    commodities = set(data['crudes']) | set(data['properties'])
    for commodity in sorted(commodities):
        model.row('BAL_' + commodity, 'E', data['inventory_change'].get(commodity, 0))
    conversion = data['barrels_per_m3']
    for name, (capacity, _, days) in data['units'].items():
        model.row('CAP_' + name, 'L', capacity * days / conversion)
    for name, (supply, price, transport, density) in data['crudes'].items():
        var = 'BUY_' + name
        model.variable(var, price * conversion / density + transport, upper=supply)
        model.add(var, 'BAL_' + name, 1)
    for name, price in data['import_price'].items():
        model.variable('IMPORT_' + name, price)
        model.add('IMPORT_' + name, 'BAL_' + name, 1)
    for name, process in data['processes'].items():
        feed, unit = process['feed'], process['unit']
        density = (data['crudes'][feed][3] if feed in data['crudes']
                   else data['properties'][feed]['density'])
        var = 'PROCESS_' + name
        model.variable(var, data['units'][unit][1] * conversion / density)
        model.add(var, 'CAP_' + unit, 1 / density)
        model.add(var, 'BAL_' + feed, -1)
        for output, fraction in process['outputs'].items():
            model.add(var, 'BAL_' + output, fraction)
    for name, transfer in data['transfers'].items():
        var = 'TRANSFER_' + name
        model.variable(var)
        model.add(var, 'BAL_' + transfer['feed'], -1)
        model.add(var, 'BAL_fuel-equiv', transfer['fuel_equiv_yield'])
    for product, (demand, price) in data['products'].items():
        model.row('PRODUCT_' + product, 'E')
        var = 'SALE_' + product
        model.variable(var, -price, demand, demand)
        model.add(var, 'PRODUCT_' + product, -1)
    for product, recipes in data['recipes'].items():
        for i, recipe in enumerate(recipes):
            if abs(sum(recipe.values()) - 1) > 1e-12:
                raise ValueError('recipe fractions must sum to one')
            var = f'RECIPE_{product}_{i}'
            model.variable(var)
            model.add(var, 'PRODUCT_' + product, 1)
            for component, fraction in recipe.items():
                model.add(var, 'BAL_' + component, -fraction)
    for product, components in data['blend_components'].items():
        for component in components:
            var = f'BLEND_{component}_{product}'
            cost = (data['lead_cost_per_m3'] / data['properties'][component]['density']
                    if product == 'motor-gas' else 0)
            model.variable(var, cost)
            model.add(var, 'BAL_' + component, -1)
            model.add(var, 'PRODUCT_' + product, 1)
    for product, side, quality, limit, basis in data['quality_specs']:
        row = f'SPEC_{product}_{side}_{quality}'
        model.row(row, 'G' if side == 'lower' else 'L')
        for component in data['blend_components'][product]:
            prop = data['properties'][component]
            scale = 1 / prop['density'] if basis == 'volume' else 1
            model.add(f'BLEND_{component}_{product}', row, (prop[quality] - limit) * scale)
    return model


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=root / 'data/refinery/fawley_public.json')
    parser.add_argument('--output', type=Path, default=root / 'examples/refinery/fawley-public.mps')
    args = parser.parse_args()
    raw = args.data.read_bytes()
    data = json.loads(raw)
    build(data).write(args.output, hashlib.sha256(raw).hexdigest(), data['flow_unit'],
                      data['objective_unit'], data['quality_unit_by_property'])


if __name__ == '__main__':
    main()
