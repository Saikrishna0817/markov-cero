# Partial W8 rerun (not acceptance evidence)

This directory is an interrupted diagnostic run, not a complete benchmark suite.
Each CSV currently has one executable hash per suite, but suite coverage is incomplete:
17/98 Netlib rows and 4/20 Mittelmann rows. The Netlib run stopped before the
suite completed; Mittelmann contains a 300-second timeout. Do not aggregate these
rows as final W8 results or compare them to the older mixed-binary 265-row files.

Every row contains the SHA-256 of the solver executable used for that row.
