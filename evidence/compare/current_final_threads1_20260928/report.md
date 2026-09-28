# Comparison: markov-cero vs HiGHS (R16 / RW-3)

- Baseline: HiGHS 1.15.1 via highspy (external oracle; never linked into the solver)
- Instances: 20 (pre-registered in `data/compare/`)
- markov-cero SHA-256: `beaed56a7534f8c8482819c1ad78aae69951c7b7ea98ef54d188bc43258e7cd1`
- markov-cero threads: 1
- Repeats per solver per cell: 2 (median; order randomized by trial); timeout penalty: 300.0s before aggregation
- markov-cero solved 20/20; verification+agreement gates passed on 20/20
- **Geometric-mean runtime ratio (markov-cero / HiGHS): 15.61x**

| instance | mc engine | mc status | hi status | mc obj | hi obj | mc median [min,max] ms | hi median [min,max] ms | ratio | agreement | pass |
|---|---|---|---|---|---|---|---|---|---|---|
| afiro | auto | Optimal | Optimal | -464.753 | -464.753 | 5.0 [3.7,6.4] | 1.5 [0.8,2.2] | 3.25x | 0.0e+00 | yes |
| blend | auto | Optimal | Optimal | -30.8121 | -30.8121 | 25.7 [23.8,27.7] | 1.6 [1.4,1.8] | 15.82x | 1.2e-16 | yes |
| sc50a | auto | Optimal | Optimal | -64.5751 | -64.5751 | 3.6 [3.0,4.3] | 0.8 [0.7,1.0] | 4.44x | 4.4e-16 | yes |
| sc50b | auto | Optimal | Optimal | -70 | -70 | 3.4 [2.8,4.1] | 0.6 [0.5,0.6] | 5.91x | 4.1e-16 | yes |
| adlittle | auto | Optimal | Optimal | 225495 | 225495 | 16.4 [16.0,16.9] | 1.4 [1.3,1.4] | 12.03x | 3.9e-16 | yes |
| share2b | auto | Optimal | Optimal | -415.732 | -415.732 | 16.6 [16.5,16.6] | 1.6 [1.5,1.8] | 10.25x | 3.3e-15 | yes |
| recipe | auto | Optimal | Optimal | -266.616 | -266.616 | 11.8 [11.7,11.9] | 1.3 [1.2,1.3] | 9.40x | 1.1e-15 | yes |
| sc105 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 11.5 [10.2,12.9] | 0.9 [0.8,1.0] | 12.49x | 5.4e-16 | yes |
| sc205 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 31.0 [31.0,31.1] | 1.8 [1.8,1.9] | 17.10x | 1.1e-15 | yes |
| kb2 | auto | Optimal | Optimal | -1749.9 | -1749.9 | 5.8 [5.7,5.9] | 0.8 [0.8,0.8] | 7.26x | 0.0e+00 | yes |
| lotfi | auto | Optimal | Optimal | -25.2647 | -25.2647 | 86.3 [84.5,88.1] | 2.1 [2.0,2.2] | 40.73x | 2.8e-16 | yes |
| beaconfd | auto | Optimal | Optimal | 33592.5 | 33592.5 | 18.1 [17.6,18.6] | 3.3 [3.2,3.3] | 5.54x | 2.2e-16 | yes |
| scsd1 | auto | Optimal | Optimal | 8.66668 | 8.66667 | 138.8 [136.4,141.2] | 3.0 [2.9,3.1] | 45.86x | 1.5e-06 | yes |
| scsd6 | auto | Optimal | Optimal | 50.5001 | 50.5 | 551.7 [545.8,557.5] | 7.5 [7.2,7.7] | 73.86x | 2.6e-06 | yes |
| share1b | auto | Optimal | Optimal | -76589.3 | -76589.3 | 72.6 [70.1,75.1] | 2.7 [2.7,2.8] | 26.51x | 4.4e-15 | yes |
| scorpion | auto | Optimal | Optimal | 1878.12 | 1878.12 | 543.9 [539.3,548.4] | 2.7 [2.5,2.9] | 200.38x | 1.2e-16 | yes |
| scagr7 | auto | Optimal | Optimal | -2.33139e+06 | -2.33139e+06 | 22.0 [21.6,22.5] | 1.7 [1.7,1.8] | 12.59x | 2.0e-16 | yes |
| stein9 | auto | Optimal | Optimal | 5 | 5 | 21.4 [21.1,21.7] | 11.3 [11.0,11.6] | 1.90x | 0.0e+00 | yes |
| stein15 | auto | Optimal | Optimal | 9 | 9 | 1084.3 [1075.1,1093.5] | 25.4 [24.6,26.1] | 42.77x | 0.0e+00 | yes |
| flugpl | auto | Optimal | Optimal | 1.2015e+06 | 1.2015e+06 | 5299.7 [5292.8,5306.6] | 71.9 [71.8,72.1] | 73.66x | 1.9e-16 | yes |

Methodology: [[Geometric Mean Runtime]] (Dolan-More 2002; Mittelmann). GM of ratios over the shared set; never compared across different sets.

**Engine fallback policy (pre-declared):** if the default engine fails or returns an unverified result, the row is retried once with `--engine pdlp`; the reported markov-cero time INCLUDES the failed attempt (total time-to-verified-answer). Rows where all engines fail are reported as failures, never dropped.
