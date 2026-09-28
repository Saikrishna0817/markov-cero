# Comparison: markov-cero vs HiGHS (R16 / RW-3)

- Baseline: HiGHS 1.15.1 via highspy (external oracle; never linked into the solver)
- Instances: 17 (pre-registered in `data/compare/`)
- Repeats per cell: 3 (median); timeout penalty: 300.0s before aggregation
- markov-cero solved 14/17; verification+agreement gates passed on 14/17
- **Geometric-mean runtime ratio (markov-cero / HiGHS): 54.67x**

| instance | mc engine | mc status | hi status | mc obj | hi obj | mc time (ms) | hi time (ms) | ratio | agreement | pass |
|---|---|---|---|---|---|---|---|---|---|---|
| afiro | auto | Optimal | Optimal | -464.753 | -464.753 | 2.9 | 1.4 | 2.07x | 0.0e+00 | yes |
| blend | pdlp | Optimal | Optimal | -30.8119 | -30.8121 | 16.8 | 1.8 | 9.38x | 8.8e-06 | yes |
| sc50a | auto | Optimal | Optimal | -64.5751 | -64.5751 | 3.7 | 1.1 | 3.32x | 2.2e-16 | yes |
| sc50b | auto | Optimal | Optimal | -70 | -70 | 3.1 | 0.9 | 3.25x | 4.1e-16 | yes |
| adlittle | auto | Optimal | Optimal | 225495 | 225495 | 13.8 | 1.7 | 7.98x | 5.2e-16 | yes |
| share2b | auto | Optimal | Optimal | -415.732 | -415.732 | 18.4 | 1.9 | 9.95x | 4.0e-15 | yes |
| recipe | auto | Optimal | Optimal | -266.616 | -266.616 | 12.5 | 1.5 | 8.61x | 1.1e-15 | yes |
| sc105 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 9.0 | 1.1 | 7.81x | 4.1e-16 | yes |
| sc205 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 61.4 | 2.1 | 29.93x | 1.2e-15 | yes |
| kb2 | pdlp | IterationLimit | Optimal | 0 | -1749.9 | 300000.0 | 0.9 | 318839.93x | nan | no |
| lotfi | pdlp | IterationLimit | Optimal | 0 | -25.2647 | 300000.0 | 3.5 | 84648.20x | nan | no |
| beaconfd | pdlp | IterationLimit | Optimal | 0 | 33592.5 | 300000.0 | 3.6 | 83372.24x | nan | no |
| scsd1 | pdlp | Optimal | Optimal | 8.66707 | 8.66667 | 26.5 | 3.9 | 6.84x | 4.7e-05 | yes |
| scsd6 | pdlp | Optimal | Optimal | 50.5034 | 50.5 | 213.8 | 9.5 | 22.52x | 6.8e-05 | yes |
| share1b | auto | Optimal | Optimal | -76589.3 | -76589.3 | 115.6 | 4.4 | 26.09x | 2.8e-15 | yes |
| scorpion | auto | Optimal | Optimal | 1878.12 | 1878.12 | 627.6 | 5.1 | 124.10x | 0.0e+00 | yes |
| scagr7 | auto | Optimal | Optimal | -2.33139e+06 | -2.33139e+06 | 20.3 | 2.2 | 9.27x | 4.0e-16 | yes |

Methodology: [[Geometric Mean Runtime]] (Dolan-More 2002; Mittelmann). GM of ratios over the shared set; never compared across different sets.

**Engine fallback policy (pre-declared):** if the default engine fails or returns an unverified result, the row is retried once with `--engine pdlp`; the reported markov-cero time INCLUDES the failed attempt (total time-to-verified-answer). Rows where all engines fail are reported as failures, never dropped.
