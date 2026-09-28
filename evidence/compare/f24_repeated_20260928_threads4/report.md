# Comparison: markov-cero vs HiGHS (R16 / RW-3)

- Baseline: HiGHS 1.15.1 via highspy (external oracle; never linked into the solver)
- Instances: 20 (pre-registered in `data/compare/`)
- markov-cero threads: 4
- Repeats per solver per cell: 2 (median; order randomized by trial); timeout penalty: 300.0s before aggregation
- markov-cero solved 20/20; verification+agreement gates passed on 20/20
- **Geometric-mean runtime ratio (markov-cero / HiGHS): 12.54x**

| instance | mc engine | mc status | hi status | mc obj | hi obj | mc median [min,max] ms | hi median [min,max] ms | ratio | agreement | pass |
|---|---|---|---|---|---|---|---|---|---|---|
| afiro | auto | Optimal | Optimal | -464.753 | -464.753 | 3.0 [1.9,4.2] | 0.6 [0.4,0.9] | 4.72x | 0.0e+00 | yes |
| blend | auto | Optimal | Optimal | -30.8121 | -30.8121 | 25.5 [23.9,27.1] | 1.5 [1.4,1.5] | 17.31x | 1.2e-16 | yes |
| sc50a | auto | Optimal | Optimal | -64.5751 | -64.5751 | 4.4 [4.3,4.5] | 0.8 [0.7,1.0] | 5.19x | 4.4e-16 | yes |
| sc50b | auto | Optimal | Optimal | -70 | -70 | 3.5 [2.9,4.2] | 0.8 [0.6,1.0] | 4.38x | 4.1e-16 | yes |
| adlittle | auto | Optimal | Optimal | 225495 | 225495 | 19.8 [17.9,21.7] | 1.8 [1.4,2.2] | 10.74x | 3.9e-16 | yes |
| share2b | auto | Optimal | Optimal | -415.732 | -415.732 | 21.8 [18.4,25.2] | 1.7 [1.5,1.8] | 13.05x | 3.3e-15 | yes |
| recipe | auto | Optimal | Optimal | -266.616 | -266.616 | 15.4 [14.6,16.2] | 1.4 [1.3,1.4] | 11.39x | 1.1e-15 | yes |
| sc105 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 10.4 [10.3,10.5] | 1.0 [0.9,1.0] | 10.78x | 5.4e-16 | yes |
| sc205 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 33.4 [32.2,34.6] | 1.9 [1.8,2.0] | 17.54x | 1.1e-15 | yes |
| kb2 | auto | Optimal | Optimal | -1749.9 | -1749.9 | 7.6 [6.3,8.9] | 0.8 [0.7,0.9] | 9.67x | 0.0e+00 | yes |
| lotfi | auto | Optimal | Optimal | -25.2647 | -25.2647 | 86.7 [86.1,87.4] | 2.2 [2.0,2.3] | 39.67x | 2.8e-16 | yes |
| beaconfd | auto | Optimal | Optimal | 33592.5 | 33592.5 | 18.1 [17.4,18.8] | 3.4 [3.3,3.4] | 5.38x | 2.2e-16 | yes |
| scsd1 | auto | Optimal | Optimal | 8.66668 | 8.66667 | 140.9 [139.5,142.2] | 3.5 [3.2,3.7] | 40.81x | 1.5e-06 | yes |
| scsd6 | auto | Optimal | Optimal | 50.5001 | 50.5 | 575.3 [560.7,589.9] | 7.8 [7.7,7.9] | 73.33x | 2.6e-06 | yes |
| share1b | auto | Optimal | Optimal | -76589.3 | -76589.3 | 75.2 [74.7,75.7] | 2.9 [2.7,3.0] | 26.14x | 4.4e-15 | yes |
| scorpion | auto | Optimal | Optimal | 1878.12 | 1878.12 | 543.8 [542.0,545.5] | 2.8 [2.7,2.9] | 194.56x | 1.2e-16 | yes |
| scagr7 | auto | Optimal | Optimal | -2.33139e+06 | -2.33139e+06 | 21.2 [21.0,21.3] | 1.8 [1.8,1.8] | 11.83x | 2.0e-16 | yes |
| stein9 | auto | Optimal | Optimal | 5 | 5 | 18.1 [16.4,19.7] | 11.3 [11.2,11.5] | 1.60x | 0.0e+00 | yes |
| stein15 | auto | Optimal | Optimal | 9 | 9 | 133.0 [63.4,202.6] | 27.0 [26.2,27.7] | 4.93x | 0.0e+00 | yes |
| flugpl | auto | Optimal | Optimal | 1.2015e+06 | 1.2015e+06 | 437.3 [412.2,462.4] | 77.7 [74.6,80.9] | 5.63x | 0.0e+00 | yes |

Methodology: [[Geometric Mean Runtime]] (Dolan-More 2002; Mittelmann). GM of ratios over the shared set; never compared across different sets.

**Engine fallback policy (pre-declared):** if the default engine fails or returns an unverified result, the row is retried once with `--engine pdlp`; the reported markov-cero time INCLUDES the failed attempt (total time-to-verified-answer). Rows where all engines fail are reported as failures, never dropped.
