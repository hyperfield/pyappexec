#!/usr/bin/env python3
"""Exercise GUI preference persistence logic."""

from __future__ import annotations

import tempfile
from pathlib import Path


def write_gui_preference(file: Path, suppress: bool) -> None:
    if suppress:
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text("suppress_gui=1\n", encoding="utf-8")
    else:
        if file.exists():
            file.unlink()


def load_gui_preference(file: Path) -> bool:
    try:
        content = file.read_text(encoding="utf-8")
    except FileNotFoundError:
        return False
    lowered = content.strip().lower()
    return "1" in lowered or "true" in lowered


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / ".pyappexec_gui_pref"

        if load_gui_preference(path):
            print("[FAIL] Fresh preference file should be absent and evaluate to False")
            return 1

        write_gui_preference(path, True)
        if not path.exists() or not load_gui_preference(path):
            print("[FAIL] Suppression flag should be persisted and readable")
            return 1

        write_gui_preference(path, False)
        if path.exists() or load_gui_preference(path):
            print("[FAIL] Clearing suppression should remove the preference file")
            return 1

    print("[ OK ] GUI preference persistence matches expectations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
