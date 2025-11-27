#!/usr/bin/env python3
"""Mirror ConfigLoader::resolveConfigPath search behavior."""

from __future__ import annotations

import os
from pathlib import Path
import tempfile


def resolve_config_path(override: str | None, cwd: Path, binary_dir: Path) -> Path:
    def search_dir(directory: Path) -> Path | None:
        if not directory or not directory.exists() or not directory.is_dir():
            return None
        primary = directory / "pyappexec.ini"
        if primary.exists():
            return primary
        for child in directory.iterdir():
            if child.is_dir():
                candidate = child / "pyappexec.ini"
                if candidate.exists():
                    return candidate
        return None

    def search_with_parents(directory: Path) -> Path | None:
        if not directory or not directory.exists():
            return None
        for _ in range(5):  # depth 0..4 inclusive
            found = search_dir(directory)
            if found:
                return found
            parent = directory.parent
            if not parent or parent == directory:
                break
            directory = parent
        return None

    if override:
        path = Path(override)
        if not path.is_absolute():
            path = cwd / path
        if not path.exists():
            raise FileNotFoundError(f"Specified config file not found: {path}")
        return path

    found_cwd = search_with_parents(cwd)
    if found_cwd:
        return found_cwd

    if binary_dir:
        binary_abs = binary_dir if binary_dir.is_absolute() else binary_dir.resolve()
        try:
            same = binary_abs.resolve() == cwd.resolve()
        except FileNotFoundError:
            same = False
        if not same:
            found_bin = search_with_parents(binary_abs)
            if found_bin:
                return found_bin

    raise RuntimeError("Config not found")


def touch_ini(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("; test\n", encoding="utf-8")
    return path


def test_absolute_override():
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        target = touch_ini(tmp_path / "explicit.ini")
        result = resolve_config_path(str(target), cwd=tmp_path, binary_dir=tmp_path / "bin")
        assert result == target


def test_relative_override():
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        target = touch_ini(tmp_path / "pyappexec.ini")
        result = resolve_config_path("pyappexec.ini", cwd=tmp_path, binary_dir=tmp_path / "bin")
        assert result == target


def test_prefers_current_tree_over_binary_tree():
    with tempfile.TemporaryDirectory() as tmp:
        cwd = Path(tmp) / "app" / "child"
        bin_dir = Path(tmp) / "bin" / "nested"
        touch_ini(cwd.parent / "pyappexec.ini")  # parent of cwd
        touch_ini(bin_dir / "pyappexec.ini")     # would be found if cwd search failed
        cwd.mkdir(parents=True, exist_ok=True)
        result = resolve_config_path(None, cwd=cwd, binary_dir=bin_dir)
        assert result == cwd.parent / "pyappexec.ini"


def test_binary_dir_used_when_cwd_missing():
    with tempfile.TemporaryDirectory() as tmp:
        cwd = Path(tmp) / "app"
        bin_dir = Path(tmp) / "bin"
        touch_ini(bin_dir / "pyappexec.ini")
        cwd.mkdir(parents=True, exist_ok=True)
        result = resolve_config_path(None, cwd=cwd, binary_dir=bin_dir)
        assert result == bin_dir / "pyappexec.ini"


def test_searches_parent_depth():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        deep = root / "a" / "b" / "c" / "d"
        touch_ini(root / "pyappexec.ini")
        deep.mkdir(parents=True, exist_ok=True)
        result = resolve_config_path(None, cwd=deep, binary_dir=root / "bin")
        assert result == root / "pyappexec.ini"


def test_missing_config_raises():
    with tempfile.TemporaryDirectory() as tmp:
        cwd = Path(tmp) / "cwd"
        bin_dir = Path(tmp) / "bin"
        cwd.mkdir()
        bin_dir.mkdir()
        try:
            resolve_config_path(None, cwd=cwd, binary_dir=bin_dir)
        except RuntimeError:
            return
        assert False, "Expected RuntimeError when no config is present"


def main() -> int:
    tests = [
        test_absolute_override,
        test_relative_override,
        test_prefers_current_tree_over_binary_tree,
        test_binary_dir_used_when_cwd_missing,
        test_searches_parent_depth,
        test_missing_config_raises,
    ]
    failures = 0
    for test_func in tests:
        try:
            test_func()
            print(f"[ OK ] {test_func.__name__}")
        except AssertionError as exc:
            failures += 1
            print(f"[FAIL] {test_func.__name__}: {exc}")
        except Exception as exc:
            failures += 1
            print(f"[FAIL] {test_func.__name__} raised unexpected error: {exc}")

    return failures


if __name__ == "__main__":
    raise SystemExit(main())
