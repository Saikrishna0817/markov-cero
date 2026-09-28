from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/profile_gpu.py')
import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import time
from typing import Dict, Any, List, Optional
