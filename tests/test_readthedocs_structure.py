#!/usr/bin/env python3
"""Ensures the Read the Docs tree has the expected files."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOCS_DIR = ROOT / "readthedocs"


REQUIRED_DOCS = {
    "conf.py",
    "index.rst",
    "introduction.rst",
    "quickstart.rst",
    "configuration.rst",
    "gui.rst",
    "advanced.rst",
    "troubleshooting.rst",
}


def main() -> int:
    errors = 0

    if not DOCS_DIR.exists():
        print(f"[FAIL] Missing directory: {DOCS_DIR}")
        return 1

    for rel_path in REQUIRED_DOCS:
        path = DOCS_DIR / rel_path
        if not path.exists():
            errors += 1
            print(f"[FAIL] Missing documentation file: {path}")

    conf = DOCS_DIR / "conf.py"
    if conf.exists():
        text = conf.read_text(encoding="utf-8")
        if "sphinx_rtd_theme" not in text:
            errors += 1
            print("[FAIL] conf.py does not configure sphinx_rtd_theme")

    if errors == 0:
        print("[ OK ] Read the Docs tree looks complete")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
