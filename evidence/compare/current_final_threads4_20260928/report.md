# Comparison: markov-cero vs HiGHS (R16 / RW-3)

- Baseline: HiGHS 1.15.1 via highspy (external oracle; never linked into the solver)
- Instances: 20 (pre-registered in `data/compare/`)
- markov-cero SHA-256: `beaed56a7534f8c8482819c1ad78aae69951c7b7ea98ef54d188bc43258e7cd1`
- markov-cero threads: 4
- Repeats per solver per cell: 2 (median; order randomized by trial); timeout penalty: 300.0s before aggregation
- markov-cero solved 20/20; verification+agreement gates passed on 20/20
- **Geometric-mean runtime ratio (markov-cero / HiGHS): 12.76x**

| instance | mc engine | mc status | hi status | mc obj | hi obj | mc median [min,max] ms | hi median [min,max] ms | ratio | agreement | pass |
|---|---|---|---|---|---|---|---|---|---|---|
| afiro | auto | Optimal | Optimal | -464.753 | -464.753 | 4.0 [3.9,4.1] | 0.6 [0.4,0.8] | 6.32x | 0.0e+00 | yes |
| blend | auto | Optimal | Optimal | -30.8121 | -30.8121 | 25.0 [24.0,26.0] | 1.5 [1.5,1.6] | 16.30x | 1.2e-16 | yes |
| sc50a | auto | Optimal | Optimal | -64.5751 | -64.5751 | 3.6 [3.4,3.8] | 0.6 [0.5,0.7] | 5.76x | 4.4e-16 | yes |
| sc50b | auto | Optimal | Optimal | -70 | -70 | 3.6 [3.2,4.0] | 0.6 [0.5,0.7] | 6.06x | 4.1e-16 | yes |
| adlittle | auto | Optimal | Optimal | 225495 | 225495 | 16.4 [16.0,16.8] | 1.5 [1.4,1.7] | 10.61x | 3.9e-16 | yes |
| share2b | auto | Optimal | Optimal | -415.732 | -415.732 | 17.7 [17.5,17.9] | 1.6 [1.5,1.8] | 10.85x | 3.3e-15 | yes |
| recipe | auto | Optimal | Optimal | -266.616 | -266.616 | 11.8 [11.6,12.0] | 1.3 [1.3,1.3] | 9.41x | 1.1e-15 | yes |
| sc105 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 9.5 [9.4,9.6] | 0.9 [0.8,0.9] | 10.76x | 5.4e-16 | yes |
| sc205 | auto | Optimal | Optimal | -52.2021 | -52.2021 | 32.1 [31.7,32.4] | 1.8 [1.8,1.8] | 17.55x | 1.1e-15 | yes |
| kb2 | auto | Optimal | Optimal | -1749.9 | -1749.9 | 5.9 [5.8,6.1] | 0.7 [0.7,0.7] | 8.53x | 0.0e+00 | yes |
| lotfi | auto | Optimal | Optimal | -25.2647 | -25.2647 | 85.5 [84.5,86.4] | 2.2 [2.0,2.3] | 39.60x | 2.8e-16 | yes |
| beaconfd | auto | Optimal | Optimal | 33592.5 | 33592.5 | 17.9 [16.6,19.3] | 3.3 [3.3,3.3] | 5.41x | 2.2e-16 | yes |
| scsd1 | auto | Optimal | Optimal | 8.66668 | 8.66667 | 137.4 [135.8,139.0] | 3.1 [3.0,3.2] | 44.00x | 1.5e-06 | yes |
| scsd6 | auto | Optimal | Optimal | 50.5001 | 50.5 | 552.5 [551.0,553.9] | 7.5 [7.3,7.7] | 73.55x | 2.6e-06 | yes |
| share1b | auto | Optimal | Optimal | -76589.3 | -76589.3 | 70.3 [70.2,70.5] | 2.8 [2.7,2.9] | 25.15x | 4.4e-15 | yes |
| scorpion | auto | Optimal | Optimal | 1878.12 | 1878.12 | 536.9 [532.2,541.6] | 2.7 [2.6,2.8] | 197.61x | 1.2e-16 | yes |
| scagr7 | auto | Optimal | Optimal | -2.33139e+06 | -2.33139e+06 | 22.9 [21.8,24.0] | 1.7 [1.7,1.7] | 13.42x | 2.0e-16 | yes |
| stein9 | auto | Optimal | Optimal | 5 | 5 | 16.8 [16.8,16.9] | 11.4 [10.8,11.9] | 1.48x | 0.0e+00 | yes |
| stein15 | auto | Optimal | Optimal | 9 | 9 | 173.2 [171.5,174.9] | 26.8 [24.8,28.8] | 6.47x | 0.0e+00 | yes |
| flugpl | auto | Optimal | Optimal | 1.2015e+06 | 1.2015e+06 | 342.1 [289.1,395.0] | 73.1 [72.5,73.6] | 4.68x | 0.0e+00 | yes |

Methodology: [[Geometric Mean Runtime]] (Dolan-More 2002; Mittelmann). GM of ratios over the shared set; never compared across different sets.

**Engine fallback policy (pre-declared):** if the default engine fails or returns an unverified result, the row is retried once with `--engine pdlp`; the reported markov-cero time INCLUDES the failed attempt (total time-to-verified-answer). Rows where all engines fail are reported as failures, never dropped.
