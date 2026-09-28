# Comparison: markov-cero vs HiGHS (R16 / RW-3)

- Baseline: HiGHS 1.15.1 via highspy (external oracle; never linked into the solver)
- Instances: 20 (pre-registered in `data/compare/`)
- markov-cero threads: 1
- Repeats per solver per cell: 1 (median; order randomized by trial); timeout penalty: 300.0s before aggregation
- markov-cero solved 20/20; verification+agreement gates passed on 20/20
- **Geometric-mean runtime ratio (markov-cero / HiGHS): 13.14x**

| instance | mc engine | mc status | hi status | mc obj | hi obj | mc median [min,max] ms | hi median [min,max] ms | ratio | agreement | pass |
|---|---|---|---|---|---|---|---|---|---|---|
| afiro | auto | Optimal | Optimal | -464.753 | -464.753 | 5.6 [5.6,5.6] | 0.9 [0.9,0.9] | 6.00x | 0.0e+00 | yes |
| blend | auto | Optimal | Optimal | -30.8121 | -30.8121 | 25.7 [25.7,25.7] | 2.6 [2.6,2.6] | 9.81x | 1.2e-16 | yes |
| sc50a | auto | Optimal | Optimal | -64.5751 | -64.5751 | 3.8 [3.8,3.8] | 0.7 [0.7,0.7] | 5.65x | 4.4e-16 | yes |
| sc50b | auto | Optimal | Optimal | -70 | -70 | 4.2 [4.2,4.2] | 0.7 [0.7,0.7] | 5.81x | 4.1e-16 | yes |
| adlittle | auto | Optimal | Optimal | 225495 | 225495 | 18.7 [18.7,18.7] | 2.6 [2.6,2.6] | 7.22x | 3.9e-16 | yes |
| share2b | auto | Optimal | Optimal | -415.732 | -415.732 | 16.8 [16.8,16.8] | 2.6 [2.6,2.6] | 6.54x | 3.3e-15 | yes |
| recipe | auto | Optimal | Optimal | -266.616 | -266.616 | 12.2 [12.2,12.2] | 1.3 [1.3,1.3] | 9.48x | 1.1e-15 | yes |
| sc105 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 12.1 [12.1,12.1] | 1.0 [1.0,1.0] | 12.07x | 5.4e-16 | yes |
| sc205 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 33.8 [33.8,33.8] | 1.8 [1.8,1.8] | 18.36x | 1.1e-15 | yes |
| kb2 | auto | Optimal | Optimal | -1749.9 | -1749.9 | 5.8 [5.8,5.8] | 1.5 [1.5,1.5] | 3.83x | 0.0e+00 | yes |
| lotfi | auto | Optimal | Optimal | -25.2647 | -25.2647 | 86.6 [86.6,86.6] | 2.4 [2.4,2.4] | 35.86x | 2.8e-16 | yes |
| beaconfd | auto | Optimal | Optimal | 33592.5 | 33592.5 | 18.0 [18.0,18.0] | 4.1 [4.1,4.1] | 4.40x | 2.2e-16 | yes |
| scsd1 | auto | Optimal | Optimal | 8.66668 | 8.66667 | 144.8 [144.8,144.8] | 3.3 [3.3,3.3] | 43.52x | 1.5e-06 | yes |
| scsd6 | auto | Optimal | Optimal | 50.5001 | 50.5 | 561.7 [561.7,561.7] | 8.0 [8.0,8.0] | 70.37x | 2.6e-06 | yes |
| share1b | auto | Optimal | Optimal | -76589.3 | -76589.3 | 77.1 [77.1,77.1] | 4.7 [4.7,4.7] | 16.58x | 4.4e-15 | yes |
| scorpion | auto | Optimal | Optimal | 1878.12 | 1878.12 | 619.7 [619.7,619.7] | 4.7 [4.7,4.7] | 132.95x | 1.2e-16 | yes |
| scagr7 | auto | Optimal | Optimal | -2.33139e+06 | -2.33139e+06 | 23.9 [23.9,23.9] | 5.6 [5.6,5.6] | 4.29x | 2.0e-16 | yes |
| stein9 | auto | Optimal | Optimal | 5 | 5 | 21.7 [21.7,21.7] | 11.3 [11.3,11.3] | 1.92x | 0.0e+00 | yes |
| stein15 | auto | Optimal | Optimal | 9 | 9 | 1169.1 [1169.1,1169.1] | 26.2 [26.2,26.2] | 44.59x | 0.0e+00 | yes |
| flugpl | auto | Optimal | Optimal | 1.2015e+06 | 1.2015e+06 | 6066.2 [6066.2,6066.2] | 73.6 [73.6,73.6] | 82.44x | 1.9e-16 | yes |

Methodology: [[Geometric Mean Runtime]] (Dolan-More 2002; Mittelmann). GM of ratios over the shared set; never compared across different sets.

**Engine fallback policy (pre-declared):** if the default engine fails or returns an unverified result, the row is retried once with `--engine pdlp`; the reported markov-cero time INCLUDES the failed attempt (total time-to-verified-answer). Rows where all engines fail are reported as failures, never dropped.
