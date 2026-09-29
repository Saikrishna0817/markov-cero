# Evidence index

This index names the evidence to consult for current capability claims. Every result is a dated observation, not a claim that this checkout has just passed the same run. Historical reports and their referenced documents remain available in Git history after repository cleanup.

For the current project narrative and release boundaries, begin with the
[repository README](../README.md), [documentation guide](../docs/README.md),
and [capability register](../docs/project/STATUS.md). This index leaves measured
reports and frozen baselines intact so their original run context remains auditable.

| Question | Primary record | Scope |
|---|---|---|
| What is the reproducible source/binary baseline? | [BASE-01 baseline manifest](baseline-manifest-20260929.json) with [raw logs](baseline-manifest-20260929/) | Clean Release build of revision `aa6f35e`: 92/92 CTest, 22/22 binding tests, installed consumer PASS, artifact hashes. No speed or breadth claim. |
| What release gates remain open? | [Gate status](gate-status-20260929.json), [readiness checkpoint](readiness-checkpoint.json), [defect closure register](defect-closure-register.csv) | Dated status and explicitly open items. |
| Which instances and comparators were frozen? | [Instance manifest](frozen-instances-20260928.json), [comparator manifest](frozen-comparators-20260928.json), [baseline freeze](baseline-freeze-20260928.json) | Pinned benchmark setup; optional large datasets are separately listed in [`data/optional-datasets.json`](../data/optional-datasets.json). |
| How did the broad local suites run? | [Netlib/MIPLIB/Mittelmann 15-second run](benchmarks/current_full_15s_20260928), [QPLIB fill-limit run](benchmarks/current_qplib_filllimit_20260928) | Solver-side 15-second cap; failures and timeouts remain in the denominator. |
| How did external solvers compare? | [Pinned five-solver report](comparison/current_glpk_pinned_20260928/full_comparison_report.md), [one-thread repeated comparison](compare/current_final_threads1_20260928/report.md), [four-thread repeated comparison](compare/current_final_threads4_20260928/report.md) | Different timing boundaries in historical runs; the reports explain their limits. |
| What was the pre-tuning comparison baseline? | [Frozen comparison report](comparison/baseline_frozen_20260928/full_comparison_report.md) | Baseline snapshot; do not replace with later runs. |
| Did GPU runs help? | [RTX 2050 host record](gpu_hardware_host_access_check_20260928.json), [reverse-order repeat](gpu_hardware_host_access_check_reverse_20260928.json), [hardware description](hardware.md) | Verified measured cases, with no end-to-end speed benefit established. |
| What are the resource and proof boundaries? | [Resource envelope](resource-envelope-20260928.json), [proof guarantee](proof-guarantee-20260928.json), [sparse-LU deadline record](ir20-sparse-lu-deadline-20260929.json) | Dated checks; cooperative deadlines and incomplete allocation coverage remain open. |
| What packaging was qualified? | [Packaging qualification](packaging-qualification-20260928.json), [prefix drill](packaging-prefix-drill-20260928.txt), [rollback drill](packaging-rollback-drill-20260928.txt) | Local offline qualification; support ownership and external review remain open. |
| What refinery data was used? | [Fawley public-input record](refinery/fawley-public.json), [units schema](refinery-units-schema-20260928.json) | Historical/public and synthetic qualification, not approved plant operating data. |

## Historical run directories

`benchmarks/`, `compare/`, and `comparison/` contain exploratory, corrected, repeated, and frozen runs. Identical CSV content in different run directories is not by itself permission to delete a snapshot: the directory name and neighboring report record experiment context. Keep each run together until its source revision, binary hash, comparator versions, instance manifest, and timing method have been copied into a durable archive index. Header-only CSVs can mean a category had no eligible rows.

The current checkout omits 227 large optional benchmark files by design. Restore only a selected hash-pinned instance with `python3 scripts/datasets.py --name NAME`; the manifest and current small fixtures remain tracked. No benchmark result should be presented as a new run without rerunning it against the current source and recording the executable hash.
