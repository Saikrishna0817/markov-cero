# Comparison: markov-cero vs HiGHS (R16 / RW-3)

- Baseline: HiGHS 1.15.1 via highspy (external oracle; never linked into the solver)
- Instances: 20 (pre-registered in `data/compare/`)
- markov-cero threads: 1
- Repeats per solver per cell: 2 (median; order randomized by trial); timeout penalty: 300.0s before aggregation
- markov-cero solved 20/20; verification+agreement gates passed on 20/20
- **Geometric-mean runtime ratio (markov-cero / HiGHS): 16.04x**

| instance | mc engine | mc status | hi status | mc obj | hi obj | mc median [min,max] ms | hi median [min,max] ms | ratio | agreement | pass |
|---|---|---|---|---|---|---|---|---|---|---|
| afiro | auto | Optimal | Optimal | -464.753 | -464.753 | 2.2 [1.8,2.7] | 0.6 [0.4,0.8] | 3.73x | 0.0e+00 | yes |
| blend | auto | Optimal | Optimal | -30.8121 | -30.8121 | 26.2 [23.8,28.5] | 1.5 [1.5,1.5] | 17.51x | 1.2e-16 | yes |
| sc50a | auto | Optimal | Optimal | -64.5751 | -64.5751 | 4.3 [4.2,4.3] | 0.8 [0.7,1.0] | 5.08x | 4.4e-16 | yes |
| sc50b | auto | Optimal | Optimal | -70 | -70 | 3.9 [3.5,4.3] | 0.9 [0.9,1.0] | 4.12x | 4.1e-16 | yes |
| adlittle | auto | Optimal | Optimal | 225495 | 225495 | 16.4 [16.2,16.6] | 1.4 [1.4,1.4] | 11.54x | 3.9e-16 | yes |
| share2b | auto | Optimal | Optimal | -415.732 | -415.732 | 19.8 [19.7,20.0] | 1.7 [1.6,1.8] | 11.83x | 3.3e-15 | yes |
| recipe | auto | Optimal | Optimal | -266.616 | -266.616 | 14.3 [12.4,16.1] | 1.3 [1.3,1.3] | 10.63x | 1.1e-15 | yes |
| sc105 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 11.2 [10.8,11.7] | 0.9 [0.9,1.0] | 11.81x | 5.4e-16 | yes |
| sc205 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 35.8 [34.0,37.6] | 1.9 [1.9,1.9] | 18.93x | 1.1e-15 | yes |
| kb2 | auto | Optimal | Optimal | -1749.9 | -1749.9 | 8.3 [7.8,8.7] | 0.9 [0.7,1.1] | 9.47x | 0.0e+00 | yes |
| lotfi | auto | Optimal | Optimal | -25.2647 | -25.2647 | 88.8 [86.7,90.9] | 2.3 [2.3,2.4] | 37.99x | 2.8e-16 | yes |
| beaconfd | auto | Optimal | Optimal | 33592.5 | 33592.5 | 18.9 [18.4,19.4] | 3.5 [3.4,3.6] | 5.43x | 2.2e-16 | yes |
| scsd1 | auto | Optimal | Optimal | 8.66668 | 8.66667 | 142.5 [140.2,144.8] | 3.3 [3.2,3.3] | 43.81x | 1.5e-06 | yes |
| scsd6 | auto | Optimal | Optimal | 50.5001 | 50.5 | 592.2 [569.5,614.8] | 8.1 [7.8,8.4] | 72.98x | 2.6e-06 | yes |
| share1b | auto | Optimal | Optimal | -76589.3 | -76589.3 | 77.6 [77.6,77.6] | 2.8 [2.8,2.8] | 28.06x | 4.4e-15 | yes |
| scorpion | auto | Optimal | Optimal | 1878.12 | 1878.12 | 558.7 [555.6,561.7] | 2.8 [2.6,3.0] | 198.35x | 1.2e-16 | yes |
| scagr7 | auto | Optimal | Optimal | -2.33139e+06 | -2.33139e+06 | 26.3 [26.1,26.5] | 1.8 [1.7,1.8] | 14.93x | 2.0e-16 | yes |
| stein9 | auto | Optimal | Optimal | 5 | 5 | 23.7 [22.6,24.8] | 12.6 [11.2,14.0] | 1.87x | 0.0e+00 | yes |
| stein15 | auto | Optimal | Optimal | 9 | 9 | 1079.4 [1073.8,1085.1] | 27.0 [26.8,27.3] | 39.91x | 0.0e+00 | yes |
| flugpl | auto | Optimal | Optimal | 1.2015e+06 | 1.2015e+06 | 5405.6 [5398.0,5413.3] | 73.5 [72.8,74.2] | 73.56x | 1.9e-16 | yes |

Methodology: [[Geometric Mean Runtime]] (Dolan-More 2002; Mittelmann). GM of ratios over the shared set; never compared across different sets.

**Engine fallback policy (pre-declared):** if the default engine fails or returns an unverified result, the row is retried once with `--engine pdlp`; the reported markov-cero time INCLUDES the failed attempt (total time-to-verified-answer). Rows where all engines fail are reported as failures, never dropped.
