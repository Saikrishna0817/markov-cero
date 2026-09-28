# Comparison: markov-cero vs HiGHS (R16 / RW-3)

- Baseline: HiGHS 1.15.1 via highspy (external oracle; never linked into the solver)
- Instances: 20 (pre-registered in `data/compare/`)
- Repeats per cell: 3 (median); timeout penalty: 300.0s before aggregation
- markov-cero solved 17/20; verification+agreement gates passed on 17/20
- **Geometric-mean runtime ratio (markov-cero / HiGHS): 42.60x**

| instance | mc engine | mc status | hi status | mc obj | hi obj | mc time (ms) | hi time (ms) | ratio | agreement | pass |
|---|---|---|---|---|---|---|---|---|---|---|
| afiro | auto | Optimal | Optimal | -464.753 | -464.753 | 2.7 | 1.1 | 2.47x | 0.0e+00 | yes |
| blend | pdlp | Optimal | Optimal | -30.8119 | -30.8121 | 16.5 | 3.1 | 5.32x | 8.8e-06 | yes |
| sc50a | auto | Optimal | Optimal | -64.5751 | -64.5751 | 3.9 | 1.2 | 3.36x | 2.2e-16 | yes |
| sc50b | auto | Optimal | Optimal | -70 | -70 | 3.0 | 1.3 | 2.22x | 4.1e-16 | yes |
| adlittle | auto | Optimal | Optimal | 225495 | 225495 | 11.2 | 2.7 | 4.15x | 5.2e-16 | yes |
| share2b | auto | Optimal | Optimal | -415.732 | -415.732 | 26.7 | 2.4 | 11.13x | 4.0e-15 | yes |
| recipe | auto | Optimal | Optimal | -266.616 | -266.616 | 17.3 | 1.8 | 9.40x | 1.1e-15 | yes |
| sc105 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 11.7 | 1.8 | 6.37x | 4.1e-16 | yes |
| sc205 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 73.1 | 2.9 | 25.44x | 1.2e-15 | yes |
| kb2 | pdlp | IterationLimit | Optimal | 0 | -1749.9 | 300000.0 | 1.7 | 175129.00x | nan | no |
| lotfi | pdlp | IterationLimit | Optimal | 0 | -25.2647 | 300000.0 | 4.3 | 69535.50x | nan | no |
| beaconfd | pdlp | IterationLimit | Optimal | 0 | 33592.5 | 300000.0 | 4.3 | 69615.13x | nan | no |
| scsd1 | pdlp | Optimal | Optimal | 8.66707 | 8.66667 | 29.1 | 3.9 | 7.50x | 4.7e-05 | yes |
| scsd6 | pdlp | Optimal | Optimal | 50.5034 | 50.5 | 245.2 | 15.3 | 16.02x | 6.8e-05 | yes |
| share1b | auto | Optimal | Optimal | -76589.3 | -76589.3 | 141.0 | 2.8 | 49.67x | 2.8e-15 | yes |
| scorpion | auto | Optimal | Optimal | 1878.12 | 1878.12 | 723.3 | 4.3 | 169.78x | 0.0e+00 | yes |
| scagr7 | auto | Optimal | Optimal | -2.33139e+06 | -2.33139e+06 | 23.4 | 2.8 | 8.44x | 4.0e-16 | yes |
| stein9 | auto | Optimal | Optimal | 5 | 5 | 40.8 | 12.7 | 3.23x | 0.0e+00 | yes |
| stein15 | auto | Optimal | Optimal | 9 | 9 | 2415.7 | 36.0 | 67.13x | 0.0e+00 | yes |
| flugpl | auto | Optimal | Optimal | 1.2015e+06 | 1.2015e+06 | 3383.8 | 94.5 | 35.81x | 1.9e-16 | yes |

Methodology: [[Geometric Mean Runtime]] (Dolan-More 2002; Mittelmann). GM of ratios over the shared set; never compared across different sets.

**Engine fallback policy (pre-declared):** if the default engine fails or returns an unverified result, the row is retried once with `--engine pdlp`; the reported markov-cero time INCLUDES the failed attempt (total time-to-verified-answer). Rows where all engines fail are reported as failures, never dropped.
