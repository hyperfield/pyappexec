#!/usr/bin/env python3
"""Runs all repository smoke tests."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


TEST_SCRIPTS = [
    "tests/test_config_structure.py",
    "tests/test_readthedocs_structure.py",
    "tests/test_build_config.py",
]


def run_script(path: Path) -> int:
    print(f"\n=== Running {path} ===")
    result = subprocess.run([sys.executable, str(path)], cwd=ROOT)
    return result.returncode


def main() -> int:
    failures = 0
    for script in TEST_SCRIPTS:
        exit_code = run_script(ROOT / script)
        if exit_code != 0:
            failures += 1
    if failures:
        print(f"\n{failures} test group(s) failed.")
        return 1
    print("\nAll tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
