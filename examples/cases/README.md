# Illustrative industrial optimization cases

These three **small, synthetic, industry-inspired** models exercise different input and solver paths. They are useful for demos and regressions, not validated production datasets. Their names describe the application idea; they do not imply an industrial partner supplied operating data.

| File | Model class | Decisions represented | Suggested route |
|---|---|---|---|
| `multiperiod_production.mps` | MILP | Production, inventory and binary setup decisions over three periods | `--engine milp` |
| `supply_chain_logistics.mps` | MILP | Facility activation and shipments to three customer zones | `--engine milp` |
| `crude_oil_blending.mps` | Convex QP | Four stream amounts, volume and quality limits, quadratic penalty | `--engine qp` |

From the repository root, build the CLI and run a case:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target markov-cero-solve --parallel 2
./build/markov-cero-solve examples/cases/multiperiod_production.mps --engine milp
./build/markov-cero-solve examples/cases/supply_chain_logistics.mps --engine milp
./build/markov-cero-solve examples/cases/crude_oil_blending.mps --engine qp
```

The solver reports a typed status, objective or bound, numerical diagnostics and verification fields. For a MILP, a feasible incumbent and a verified global optimum are distinct outcomes; proof export/replay has separate budgets. For QP, acceptance depends on convexity and KKT checks. See the [verification guide](../../docs/guides/VERIFY.md) and [current status](../../docs/project/STATUS.md) before citing any result. Earlier objective, runtime and node-count examples were environment-specific snapshots and are not maintained as current performance claims here.

The [refinery qualification models](../refinery/README.md) provide a separate scenario with feasible, infeasible, malformed and limited outcomes; the public historical input has its own provenance caveats.
