#!/usr/bin/env python3
"""Smoke-test the end-to-end launcher provisioning path with a tiny project."""

from __future__ import annotations

import platform
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def find_launcher() -> Path | None:
    suffix = ".exe" if platform.system() == "Windows" else ""
    candidates = [
        ROOT / "build" / "Release" / f"pyappexec{suffix}",
        ROOT / "build" / f"pyappexec{suffix}",
        ROOT / f"pyappexec{suffix}",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def write_ini(base: Path) -> Path:
    os_prefix = {"Windows": "Windows", "Darwin": "MacOS"}.get(platform.system(), "Linux")
    app_dir = base
    config_root = base / ".state"
    venv = config_root / "venv"
    ini = base / "pyappexec.ini"
    ini.write_text(
        "\n".join(
            [
                f"[{os_prefix}:main]",
                "python_download_url = ",
                "python_min_ver = 3.8",
                "app_id = TESTAPP01",
                f"config_root = {config_root}",
                f"python_app_dir = {app_dir}",
                "exec_app_path = main.py",
                "requirements_file = ",
                f"virtual_env_dir = {venv}",
                "GUI = false",
                "log_console = true",
                "log_level = info",
                "",
                f"[{os_prefix}:requirements]",
                "requirement_1 = ",
                "requirement_1_url = ",
            ]
        ),
        encoding="utf-8",
    )
    return ini


def main() -> int:
    launcher = find_launcher()
    if not launcher:
        print("[WARN] Launcher binary not built; skipping provisioning smoke test")
        return 0

    with tempfile.TemporaryDirectory() as tmp:
        base = Path(tmp)
        main_py = base / "main.py"
        main_py.write_text("print('hello from pyappexec smoke test')\n", encoding="utf-8")
        ini = write_ini(base)

        result = subprocess.run(
            [str(launcher), "--no-gui", "--config", str(ini)],
            cwd=base,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=120,
        )

        if result.returncode != 0:
            print("[FAIL] Provisioning run failed")
            print(result.stdout)
            return 1

    print("[ OK ] Provisioning smoke test completed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
