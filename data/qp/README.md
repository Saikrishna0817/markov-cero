# QPLIB quadratic-program examples

Four small QPLIB inputs are tracked in raw `.qplib` form and as converted MPS with `QUADOBJ`: `QPLIB_0001`, `QPLIB_0002`, `QPLIB_0010` and `QPLIB_0025`. Their `.provenance.json` sidecars record input/conversion metadata. Other named QPLIB instances have provenance records but their larger materialized files are optional; see the [dataset manifest](../optional-datasets.json). The local collection is not the full upstream QPLIB library.

[`scripts/import_qplib.py`](../../scripts/import_qplib.py) performs conversion. It parses objective sense, bounds, rows and quadratic terms, then writes MPS and provenance metadata. Converted MPS is a **derived input**, so verify raw hashes and conversion settings before comparing a new run with a historical one.

From the repository root:

```sh
./build/markov-cero-solve data/qp/QPLIB_0001.mps --engine qp
python3 scripts/datasets.py --list
```

Earlier runs of these four examples reported optimal solutions with QP KKT verification. Those objectives and runtimes are dated results, not a guarantee for every QPLIB problem or for a changed binary. The [evidence index](../../evidence/INDEX.md) and [status register](../../docs/project/STATUS.md) describe current verification and benchmark coverage. Consult the upstream collection terms before redistributing source models.
