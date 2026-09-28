# Mittelmann published-table reference (D-09)

Source: https://plato.asu.edu/ftp/milp_tables/12threads.res (accessed 2026-09-27)
Instances: 242 (MIPLIB2017 benchmark, preprocessed)

| solver | solved/total |
|---|---|
| COPT | 221/242 |
| FiberSCIP-cpx | 191/242 |
| HiGHS | 160/242 |
| HiGHSp | 183/242 |
| optverse | 216/242 |
| SCIP-spx | 138/242 |
| SCIP-conc | 156/242 |

markov-cero does not appear in these published tables; the local HiGHS comparison (results CSVs above) is the only measured baseline. Published numbers provide the commercial-solver context without licenses.

## Per-instance published entries (curated data/mittelmann set, M6-37)

Instance names are matched exactly against `data/mittelmann_tables/milp_12threads.csv` after stripping the published `p_` prefix.  The published tables record **wall-clock times only** - they carry no objective values, so every objective entry below is marked unverified unless a provenance file supplies a `reference_objective`.

| instance | published entry | published times (s) | reference objective | verified |
|---|---|---|---|---|
| markshare_5_0 | no exact name match | - | - | unverified (no published/provenance objective) |
| bienst1 | no exact name match | - | - | unverified (no published/provenance objective) |
| neos5 | p_neos5 (milp_12threads.csv) | COPT 18, FiberSCIP-cpx 68, HiGHS 66, HiGHSp 55, optverse 14, SCIP-spx 370, SCIP-conc 447 | - | unverified (no published/provenance objective) |
| ran14x18_1 | no exact name match | - | - | unverified (no published/provenance objective) |
