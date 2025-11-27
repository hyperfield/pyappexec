#!/usr/bin/env python3
"""Ensure the launcher binary responds to --help without side effects."""

from __future__ import annotations

import platform
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def candidate_binaries() -> list[Path]:
    suffix = ".exe" if platform.system() == "Windows" else ""
    return [
        ROOT / "build" / "Release" / f"pyappexec{suffix}",
        ROOT / "build" / f"pyappexec{suffix}",
        ROOT / f"pyappexec{suffix}",
    ]


def main() -> int:
    binary = next((p for p in candidate_binaries() if p.exists()), None)
    if not binary:
        print("[WARN] Launcher binary not found; skipping --help smoke test")
        return 0

    result = subprocess.run([str(binary), "--help"], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[FAIL] {binary} --help exited with {result.returncode}")
        print(result.stdout)
        print(result.stderr)
        return 1

    expected_tokens = ["--config", "--no-gui", "--reset-gui", "--help"]
    if not all(token in result.stdout for token in expected_tokens):
        print("[FAIL] Help output missing expected flags")
        print(result.stdout)
        return 1

    print(f"[ OK ] {binary} --help succeeded")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
