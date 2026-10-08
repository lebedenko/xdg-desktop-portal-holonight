#!/usr/bin/env python3
"""Check private C++ sources using a Clang-compatible copy of the build database."""
import concurrent.futures
import json
from pathlib import Path
import shlex
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    build = Path(sys.argv[1]).resolve()
    destination = build / "tidy"
    destination.mkdir(exist_ok=True)
    database = json.loads((build / "compile_commands.json").read_text())
    ignored = {"-mno-direct-extern-access", "-fno-keep-inline-dllexport", "-Wno-template-id-cdtor"}
    for entry in database:
        arguments = entry.get("arguments", shlex.split(entry.get("command", "")))
        entry["arguments"] = [argument for argument in arguments if argument not in ignored]
        entry.pop("command", None)
    (destination / "compile_commands.json").write_text(json.dumps(database))
    sources = sorted(path for folder in ("src", "tests") for path in (root / folder).glob("*.cpp"))

    def check(path):
        command = ["clang-tidy", "-p", str(destination), str(path)]
        result = subprocess.run(command, capture_output=True, text=True)
        (destination / f"{path.stem}.log").write_text(result.stdout + result.stderr)
        return path, result

    failed = False
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for path, result in pool.map(check, sources):
            print(f"{path.relative_to(root)}: {'PASS' if result.returncode == 0 else 'FAIL'}", flush=True)
            if result.returncode:
                failed = True
                print(result.stdout + result.stderr)
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
