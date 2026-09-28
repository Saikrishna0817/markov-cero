from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/run_gpu.py')
import argparse
import csv
import datetime
import hashlib
import json
import math
import os
import platform
import shutil
import subprocess
import sys
import textwrap
import time
from typing import Dict, Any, List, Optional
BENCHMARKS = {
    "afiro": {"rows": 28, "cols": 32, "optimal": -464.753142857143},
    "blend": {"rows": 75, "cols": 84, "optimal": -30.8121498457},
    "adlittle": {"rows": 57, "cols": 97, "optimal": 225494.96316238},
    "sc50a": {"rows": 51, "cols": 48, "optimal": -64.575077058564},
    "sc50b": {"rows": 51, "cols": 48, "optimal": -70.000000000000},
    "sc105": {"rows": 106, "cols": 103, "optimal": -52.202061211707},
    "sc205": {"rows": 206, "cols": 203, "optimal": -52.202061211707},
    "share2b": {"rows": 97, "cols": 79, "optimal": -415.732240741418},
    "share1b": {"rows": 118, "cols": 225, "optimal": -76589.3185791855},
    "recipe": {"rows": 92, "cols": 180, "optimal": -266.616000000000},
    "scsd1": {"rows": 78, "cols": 760, "optimal": 8.66666667},
    "scsd6": {"rows": 148, "cols": 1350, "optimal": 50.5},
    "beaconfd": {"rows": 174, "cols": 262, "optimal": 33592.4858072000},
    "scorpion": {"rows": 389, "cols": 358, "optimal": 1878.1248227381},
    # Scale-study instances (data/scale_study, objectives per
    # evidence/benchmarks/crossover_study.csv reference_objective column)
    "scale_5": {"rows": 6, "cols": 8, "optimal": 9.999981268643653},
    "scale_10": {"rows": 9, "cols": 12, "optimal": 9.999981268643653},
    "scale_20": {"rows": 16, "cols": 24, "optimal": 14.999975797264936},
    "scale_35": {"rows": 25, "cols": 40, "optimal": 19.99997166131662},
    "scale_50": {"rows": 42, "cols": 70, "optimal": 24.999968666875546},
    "scale_75": {"rows": 56, "cols": 96, "optimal": 29.999966716397292},
    "scale_100": {"rows": 60, "cols": 100, "optimal": 24.999968666875546},
    "scale_200": {"rows": 126, "cols": 224, "optimal": 39.999965556864396},
    "scale_500": {"rows": 275, "cols": 500, "optimal": 49.99996766091429},
    "scale_1000": {"rows": 525, "cols": 980, "optimal": 69.99998065148145},
    "scale_2000": {"rows": 1050, "cols": 2000, "optimal": 100.0000494963771},
    "scale_5000": {"rows": 2550, "cols": 4950, "optimal": 165.00031099724058},
    "scale_10000": {"rows": 5100, "cols": 10000, "optimal": 250.00051979740016},
}
DEFAULT_INSTANCES = ["afiro", "blend", "sc50a", "sc50b", "adlittle", "scsd1"]
RECORD_FIELDS = [
    "instance", "rows", "cols", "nonzeros", "reference_objective",
    "simplex_status", "simplex_verified", "simplex_objective",
    "simplex_iterations", "simplex_time_ms",
    "cpu_pdlp_status", "cpu_pdlp_verified", "cpu_pdlp_objective",
    "cpu_pdlp_iterations", "cpu_pdlp_time_ms",
    "gpu_pdlp_status", "gpu_pdlp_objective", "gpu_pdlp_iterations",
    "gpu_h2d_ms", "gpu_kernel_ms", "gpu_d2h_ms", "gpu_total_ms",
    "speedup_kernel", "speedup_end_to_end", "speedup_vs_simplex",
    "gpu_verified", "failures", "hardware_id", "pass",
]
TABLE_WIDTH = 152
