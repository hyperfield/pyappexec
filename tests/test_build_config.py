#!/usr/bin/env python3
"""Smoke-test CMakeLists for key dependencies."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CMAKE_FILE = ROOT / "CMakeLists.txt"


def main() -> int:
    if not CMAKE_FILE.exists():
        print(f"[FAIL] Missing {CMAKE_FILE}")
        return 1

    content = CMAKE_FILE.read_text(encoding="utf-8")
    errors = 0

    expected_snippets = [
        "set(CMAKE_AUTOMOC ON)",
        "find_package(Qt6",  # Qt6 discovery present
        "COMPONENTS Widgets",  # Widgets component requested
        "gui/MainWindow.cpp",
        "gui/GuiRunner.cpp",
        "Qt6::Widgets",
        "find_package(spdlog",
        "spdlog::spdlog",
    ]

    for snippet in expected_snippets:
        if snippet not in content:
            errors += 1
            print(f"[FAIL] CMakeLists.txt is missing '{snippet}'")

    if errors == 0:
        print("[ OK ] CMakeLists.txt contains GUI build configuration")

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
