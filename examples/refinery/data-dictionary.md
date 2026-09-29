# Refinery model data dictionary

This dictionary describes the **synthetic continuous LP** used by
`refinery-feasible.mps` and the qualification demo. It is not an MRPL production
model, a bill of materials, or a refinery engineer's approved unit balance.
Quantities are expressed in the model's canonical units and linear yields;
there is no integer unit commitment or nonlinear blending-index calculation.

| Name | Kind | Meaning | Units |
|---|---|---|---|
| A, B | variables | Purchase/process of two crudes | t |
| COST | objective | Minimize purchase cost | currency |
| CDU | row | Combined crude distillation throughput | t |
| AVAIL_A, AVAIL_B | rows | Crude availability | t |
| PETROL, DIESEL, ATF | rows | Minimum product yields | t |
| SULFUR | row | Blend sulfur mass | t |

The separate `fawley-public.mps` example is derived from historical public
inputs and has its own [source record](../../data/refinery/fawley_public.json).
Its approximate blend indices and historical costs should not be conflated
with the synthetic variables in this table. Read the
[case guide](README.md) and [project status](../../docs/project/STATUS.md)
before presenting either model as a planning result.
