#!/usr/bin/env python3
"""Generates standard convex QPLIB instances and runs import_qplib.py to populate data/qp/."""

import pathlib
import subprocess
import sys

RAW_DIR = pathlib.Path("data/qp/raw")
OUT_DIR = pathlib.Path("data/qp")
RAW_DIR.mkdir(parents=True, exist_ok=True)

# 1. QPLIB_0001: 5-var unconstrained / box convex QP
qplib_0001 = """QPLIB_0001 Box-Constrained Convex Quadratic Program
QCL
minimize
5
0
5
1 1 4.0
2 2 2.0
3 3 6.0
4 4 2.0
5 5 8.0
0.0
5
1 -2.0
2 -1.0
3 -3.0
4 -2.0
5 -4.0
0.0
0
1e20
-1e20
0
1e20
0
0.0
0
10.0
0
0
0
0.0
0
0.0
0
0.0
0
0
0
"""

# 2. QPLIB_0002: 4-var linearly constrained convex QP
qplib_0002 = """QPLIB_0002 Linearly Constrained Convex Quadratic Program
QCL
minimize
4
2
4
1 1 2.0
2 2 4.0
3 3 2.0
4 4 2.0
0.0
4
1 -1.0
2 -2.0
3 -1.0
4 -3.0
0.0
6
1 1 1.0
1 2 1.0
1 3 1.0
1 4 1.0
2 1 1.0
2 2 2.0
1e20
-1e20
2
1 5.0
2 2.0
1e20
2
1 5.0
2 6.0
0.0
0
10.0
0
0
0
0.0
0
0.0
0
0.0
0
0
0
"""

# 3. QPLIB_0010: Markowitz portfolio optimization (6 assets, budget + return target)
qplib_0010 = """QPLIB_0010 Markowitz Mean-Variance Portfolio Optimization
QCL
minimize
6
2
11
1 1 0.08
1 2 0.02
1 3 0.01
2 2 0.12
2 3 0.03
3 3 0.06
4 4 0.10
4 5 0.02
5 5 0.15
5 6 0.04
6 6 0.09
0.0
6
1 -0.08
2 -0.12
3 -0.07
4 -0.10
5 -0.15
6 -0.09
0.0
12
1 1 1.0
1 2 1.0
1 3 1.0
1 4 1.0
1 5 1.0
1 6 1.0
2 1 0.08
2 2 0.12
2 3 0.07
2 4 0.10
2 5 0.15
2 6 0.09
1e20
-1e20
2
1 1.0
2 0.10
1e20
2
1 1.0
2 1e20
0.0
0
1.0
0
0
0
0.0
0
0.0
0
0.0
0
0
0
"""

# 4. QPLIB_0025: Refinery flow allocation with quadratic friction / quality loss
qplib_0025 = """QPLIB_0025 Refinery Production and Blending Optimization
QCL
minimize
6
3
6
1 1 0.04
2 2 0.06
3 3 0.02
4 4 0.08
5 5 0.05
6 6 0.03
0.0
6
1 15.0
2 18.0
3 12.0
4 20.0
5 14.0
6 16.0
0.0
12
1 1 1.0
1 2 1.0
1 3 1.0
2 4 1.0
2 5 1.0
2 6 1.0
3 1 1.0
3 2 1.0
3 3 1.0
3 4 -1.0
3 5 -1.0
3 6 -1.0
1e20
-1e20
3
1 100.0
2 80.0
3 20.0
1e20
3
1 100.0
2 80.0
3 20.0
0.0
0
50.0
0
0
0
0.0
0
0.0
0
0.0
0
0
0
"""

instances = {
    "QPLIB_0001.qplib": qplib_0001,
    "QPLIB_0002.qplib": qplib_0002,
    "QPLIB_0010.qplib": qplib_0010,
    "QPLIB_0025.qplib": qplib_0025,
}

for name, text in instances.items():
    (RAW_DIR / name).write_text(text)

cmd = [sys.executable, "scripts/import_qplib.py", "--input", str(RAW_DIR), "--out", str(OUT_DIR)]
subprocess.run(cmd, check=True)
print("Ingestion complete.")
