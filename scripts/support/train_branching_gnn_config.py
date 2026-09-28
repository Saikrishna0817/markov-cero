from __future__ import annotations
from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/ml/train_branching_gnn.py')
import argparse
import hashlib
import json
import sys
import time
from pathlib import Path
import numpy as np
import torch
from torch import nn
import onnx
import onnxruntime as ort
sys.path.insert(0, str(Path(_ENTRY_POINT).resolve().parent))
import feature_spec
import ml_data
import ml_metrics
import ml_splits
ROOT = Path(_ENTRY_POINT).resolve().parents[2]
HIDDEN = 32
