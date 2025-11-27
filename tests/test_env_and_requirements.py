#!/usr/bin/env python3
"""Check env expansion semantics and requirement numbering."""

from __future__ import annotations

import configparser
import os
import platform
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INI_PATH = ROOT / "pyappexec.ini"


def expand_environment(value: str) -> str:
    if platform.system() != "Windows":
        return value
    # Match the Windows-only expansion behavior used by the launcher.
    return os.path.expandvars(value)


def requirement_sections(parser: configparser.ConfigParser) -> list[tuple[str, str]]:
    sections: list[tuple[str, str]] = []
    for section in parser.sections():
        if ":requirements" in section:
            sections.append((section, parser[section].get("requirement_1", "")))
    return sections


def validate_contiguous_requirements(parser: configparser.ConfigParser) -> list[str]:
    errors: list[str] = []
    for section in parser.sections():
        if not section.endswith(":requirements"):
            continue
        cfg = parser[section]
        index = 1
        while True:
            key = f"requirement_{index}"
            next_key = f"requirement_{index + 1}"
            present = cfg.get(key, "")
            if not present:
                if cfg.get(next_key, ""):
                    errors.append(f"{section}: numbering must be contiguous; missing {key}")
                break
            index += 1
    return errors


def main() -> int:
    if not INI_PATH.exists():
        print(f"[FAIL] Missing configuration file at {INI_PATH}")
        return 1

    parser = configparser.ConfigParser(interpolation=None)
    parser.read(INI_PATH)

    errors = validate_contiguous_requirements(parser)
    if errors:
        for err in errors:
            print(f"[FAIL] {err}")
        return 1

    # Ensure Windows FFmpeg paths use env expansion tokens instead of hardcoded usernames.
    win_reqs = parser["Windows:requirements"]
    version_cmd = win_reqs.get("requirement_1_version_check_command", "")
    install_dir = win_reqs.get("requirement_1_install_dir", "")
    if "%USERPROFILE%" not in version_cmd or "%USERPROFILE%" not in install_dir:
        print("[FAIL] Windows requirement paths should reference %USERPROFILE% for the current user")
        return 1

    # Verify expansion produces a concrete path on Windows; on other OSes the string should pass through unchanged.
    os.environ.setdefault("USERPROFILE", str(Path.home()))
    expanded = expand_environment(version_cmd)
    if platform.system() == "Windows":
        if "%USERPROFILE%" in expanded:
            print("[FAIL] Expected Windows environment expansion to replace %USERPROFILE%")
            return 1
    else:
        if expanded != version_cmd:
            print("[FAIL] Non-Windows platforms should not expand percent-encoded variables")
            return 1

    print("[ OK ] Requirement numbering and environment expansion validated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
