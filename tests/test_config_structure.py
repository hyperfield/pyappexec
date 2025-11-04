#!/usr/bin/env python3
"""Static validation for pyappexec.ini."""

from __future__ import annotations

import configparser
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INI_PATH = ROOT / "pyappexec.ini"


def fail(message: str) -> None:
    print(f"[FAIL] {message}")


def pass_msg(message: str) -> None:
    print(f"[ OK ] {message}")


def validate() -> int:
    if not INI_PATH.exists():
        fail(f"Missing configuration file: {INI_PATH}")
        return 1

    parser = configparser.ConfigParser()
    parser.read(INI_PATH)

    errors = 0
    config_dir = INI_PATH.parent
    os_families = ["Linux", "Windows", "MacOS"]

    for os_name in os_families:
        main_section = f"{os_name}:main"
        req_section = f"{os_name}:requirements"

        if main_section not in parser:
            errors += 1
            fail(f"Missing section [{main_section}]")
            continue

        section = parser[main_section]

        required_keys = ["python_min_ver", "python_app_dir", "exec_app_path", "virtual_env_dir"]
        for key in required_keys:
            if not section.get(key):
                errors += 1
                fail(f"[{main_section}] missing required key '{key}'")

        python_app_dir = section.get("python_app_dir", fallback="")
        app_dir_path = (config_dir / python_app_dir).resolve() if python_app_dir else None
        if app_dir_path and not app_dir_path.exists():
            errors += 1
            fail(f"[{main_section}] python_app_dir path does not exist: {app_dir_path}")

        exec_app_path = section.get("exec_app_path", fallback="")
        if app_dir_path and exec_app_path:
            exec_path = (app_dir_path / exec_app_path).resolve()
            if not exec_path.exists():
                errors += 1
                fail(f"[{main_section}] exec_app_path not found: {exec_path}")

        req_file = section.get("requirements_file", fallback="")
        if app_dir_path and req_file:
            req_path = (app_dir_path / req_file).resolve()
            if not req_path.exists():
                errors += 1
                fail(f"[{main_section}] requirements_file not found: {req_path}")

        gui_value = section.get("GUI", fallback="")
        if gui_value:
            normalized = gui_value.strip().lower()
            if normalized not in {"true", "false"}:
                errors += 1
                fail(f"[{main_section}] GUI must be true/false, found '{gui_value}'")

        log_console_value = section.get("log_console", "")
        if log_console_value and log_console_value.strip().lower() not in {"true", "false"}:
            errors += 1
            fail(f"[{main_section}] log_console must be true/false")

        log_level_value = section.get("log_level", "")
        valid_levels = {"trace", "debug", "info", "warn", "warning", "error", "critical", "fatal", "off", "none"}
        if log_level_value and log_level_value.strip().lower() not in valid_levels:
            errors += 1
            fail(f"[{main_section}] log_level has invalid value '{log_level_value}'")

        if req_section not in parser:
            errors += 1
            fail(f"Missing section [{req_section}]")
            continue

        req_config = parser[req_section]
        index = 1
        while True:
            key = f"requirement_{index}"
            if key not in req_config:
                if f"requirement_{index+1}" in req_config:
                    errors += 1
                    fail(f"[{req_section}] numbering must be contiguous; missing {key}")
                break
            name = req_config.get(key, "").strip()
            if not name:
                errors += 1
                fail(f"[{req_section}] {key} must have a non-empty name")
            index += 1

    if errors == 0:
        pass_msg("pyappexec.ini structure looks good")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(validate())
