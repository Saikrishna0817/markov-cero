# Mittelmann benchmark subset

This directory contains a **small local subset** of models selected for solver evaluation, plus provenance sidecars for optional instances. Three MPS inputs are tracked here (`markshare_5_0`, `neos5`, `ran14x18_1`); other named models in the provenance records may need restoration through the [optional dataset manifest](../optional-datasets.json). This is not a mirror of a complete upstream benchmark collection.

The subset combines integer planning/assignment examples with difficult LP relaxations. It helps exercise parser behavior, branch-and-cut, sparse factorization and numerical stop conditions. A typed stop on a singular or difficult instance is an outcome to investigate, not an optimality result.

```sh
python3 scripts/datasets.py --list
./build/markov-cero-solve data/mittelmann/markshare_5_0.mps --engine milp
```

Run commands from the repository root. The [current local benchmark sweep](../../evidence/INDEX.md) records the source and binary used, time caps, attempted rows, failures and verified outcomes. Do not compare a new result to the historical CSV without matching the instance hash and method. The `.provenance.json` sidecars and manifest are the local integrity sources; consult the upstream dataset terms before redistributing a restored model.
