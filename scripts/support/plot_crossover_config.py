from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/plot_crossover.py')
import argparse
import csv
import math
import os
import sys
from typing import List, Dict, Any, Optional
SOLVED_STATUS = "Optimal"
SERIES_ORDER = ["simplex", "cpu_pdlp", "gpu"]
BACKEND_TO_SERIES = {
    "cpu_simplex": "simplex",
    "cpu_pdlp": "cpu_pdlp",
    "gpu_pdlp": "gpu",
    # legacy rows from the pre-scale-up study (--engine pdlp for both rows)
    "cpu": "cpu_pdlp",
    "gpu": "gpu",
}
SERIES_STYLE = {
    "simplex": {"label": "CPU Simplex (O(m^2.5) pivoting)",
                "color": "#ef4444", "dash": "", "radius": 4.0},
    "cpu_pdlp": {"label": "CPU PDLP (single-thread first-order)",
                 "color": "#10b981", "dash": "4,2", "radius": 3.5},
    "gpu": {"label": "GPU PDLP (CUDA SpMV + H2D/D2H)",
            "color": "#38bdf8", "dash": "", "radius": 4.5},
}
