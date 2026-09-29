# Refinery qualification examples

This folder provides small models for demonstrating the solver's input, solve and verification path. Four synthetic cases exercise distinct outcomes. A fifth MPS file is derived from attributed historical public Fawley inputs; it is a mathematical qualification model, **not** approved MRPL plant data or an operational schedule.

| File | Purpose | Expected kind of result |
|---|---|---|
| `refinery-feasible.mps` | Two-crude CDU blend with product and sulfur constraints | Verified optimum for the synthetic case |
| `refinery-infeasible.mps` | Deliberately inconsistent constraints | Checked infeasibility witness when supported |
| `refinery-malformed.mps` | Invalid input structure | Typed `InvalidModel` failure |
| `refinery-limited.mps` | Demonstrate a solver limit | `IterationLimit` with `--iteration-limit 1` |
| `fawley-public.mps` | Historical public-input qualification | Objective and feasibility checks for this generated model |

From the **repository root**:

```sh
bash scripts/run-qualification-demo.sh
./build/markov-cero-solve examples/refinery/fawley-public.mps
```

The demo script builds the CLI if necessary, solves the synthetic feasible model and prints its JSON output. The Fawley MPS is generated from [`data/refinery/fawley_public.json`](../../data/refinery/fawley_public.json) using [`scripts/generators/gen_public_refinery.py`](../../scripts/generators/gen_public_refinery.py). Its [data dictionary](data-dictionary.md) explains units and fields, and [expected results](expected-results.json) describe the qualification checks. The public data include approximate blend indices and historical assumptions; review [provenance](../../docs/project/PROVENANCE.md) before using the result in a presentation.

A recorded local markov-cero run and development HiGHS oracle agreed on objective −2899.252790423 thousand historical USD per period for the 29-row, 36-column generated model. This agreement does not validate the physical formulation. Engineer review of mass and quality balance, approved operating data and an agreed shadow trial remain open [release gates](../../evidence/gate-status-20260929.json).
