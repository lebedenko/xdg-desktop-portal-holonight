#!/usr/bin/env python3
"""Use the repository's recursive C++/QML source inventory for formatting checks."""
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    sources = sorted(path for folder in ("src", "qml", "tests") for path in (root / folder).rglob("*")
                     if path.is_file() and path.suffix in (".cpp", ".h", ".qml"))
    subprocess.run(["clang-format", "--dry-run", "--Werror", *(str(path) for path in sources if path.suffix != ".qml")],
                   check=True)
    qt6_formatter = Path("/usr/lib/qt6/bin/qmlformat")
    formatter = os.environ.get("QMLFORMAT") or (str(qt6_formatter) if qt6_formatter.is_file() else shutil.which("qmlformat"))
    if not formatter:
        sys.exit("Set QMLFORMAT to the configured Qt toolchain’s qmlformat executable.")
    for path in sources:
        if path.suffix == ".qml":
            result = subprocess.run([formatter, str(path)], capture_output=True, check=True)
            if result.stdout != path.read_bytes():
                sys.exit(f"QML formatting differs: {path}")


if __name__ == "__main__":
    main()
