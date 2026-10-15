#!/usr/bin/env python3
"""Check canonical C++ and QML formatting."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
result = subprocess.call([sys.executable, str(root / 'scripts/quality.py'), 'format-check'], cwd=root)
if result:
    sys.exit(result)
preset = next((p for p in ('dev', 'release')
               if (root / 'build' / p / 'CMakeCache.txt').is_file()), 'dev')
formatter = subprocess.check_output([sys.executable, str(root / "scripts/qt-tool.py"), "--tool", "qmlformat", "--preset", preset], cwd=root, text=True).strip()
for path in (root / "qml").rglob("*.qml"):
    if subprocess.check_output([formatter, str(path)]) != path.read_bytes():
        sys.exit(f"QML formatting differs: {path}")
