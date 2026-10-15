#!/usr/bin/env python3
"""Run strict analysis against the supplied CMake build directory."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(root / 'scripts/quality.py'), 'tidy', '--build', sys.argv[1]], check=True)
