#!/usr/bin/env python3
"""Run canonical strict tidy using the supplied build database."""
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tooling'))
import workflow


def main():
    if len(sys.argv) != 2:
        sys.exit('Usage: scripts/check-cpp.py BUILD_DIRECTORY')
    database = Path(sys.argv[1]).resolve() / 'compile_commands.json'
    # Preserve the legacy entrypoint's explicit compiler context. Canonical tidy
    # still owns source inventory, strict configuration and Clang flag filtering.
    entries = json.loads(database.read_text())
    workflow.refresh = lambda config: workflow.atomic_json(workflow.ROOT / 'compile_commands.json', entries)
    workflow.tidy(workflow.settings(), 'all')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
