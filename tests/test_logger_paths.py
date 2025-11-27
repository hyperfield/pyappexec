#!/usr/bin/env python3
"""Validate user log path calculation mirrors the launcher defaults."""

from __future__ import annotations

import os
import platform
from pathlib import Path


def expected_log_dir() -> Path:
    system = platform.system()
    if system == "Windows":
        local_app_data = os.getenv("LOCALAPPDATA")
        app_data = os.getenv("APPDATA")
        if local_app_data:
            return Path(local_app_data) / "PyAppExec" / "logs"
        if app_data:
            return Path(app_data) / "PyAppExec" / "logs"
        return Path("logs")
    if system == "Darwin":
        home = os.getenv("HOME")
        base = Path(home) if home else Path.cwd()
        return base / "Library" / "Logs" / "PyAppExec"

    xdg_state = os.getenv("XDG_STATE_HOME")
    if xdg_state:
        return Path(xdg_state) / "pyappexec"
    home = os.getenv("HOME")
    base = Path(home) if home else Path.cwd()
    return base / ".local" / "state" / "pyappexec"


def main() -> int:
    log_dir = expected_log_dir()
    log_file = log_dir / "pyappexec.log"

    # Ensure the chosen directory is creatable.
    try:
        log_dir.mkdir(parents=True, exist_ok=True)
    except Exception as exc:  # pragma: no cover - we want explicit failure reporting
        print(f"[FAIL] Could not create log directory {log_dir}: {exc}")
        return 1

    if not log_dir.exists() or not log_dir.is_dir():
        print(f"[FAIL] Expected log directory missing: {log_dir}")
        return 1

    expected_suffix = Path("pyappexec.log")
    if log_file.name != expected_suffix.name:
        print(f"[FAIL] Unexpected log filename: {log_file}")
        return 1

    print(f"[ OK ] Log directory resolved to {log_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
