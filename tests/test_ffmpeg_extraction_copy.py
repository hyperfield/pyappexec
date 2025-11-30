#!/usr/bin/env python3
"""Validate the Windows extraction script copies ffmpeg/ffprobe to the install root."""

from __future__ import annotations

import os
import platform
import subprocess
import sys
import tempfile
from pathlib import Path
from zipfile import ZipFile


def powershell_path() -> str:
    system_root = os.environ.get("SystemRoot") or os.environ.get("WINDIR")
    if system_root:
        candidate = Path(system_root) / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe"
        if candidate.exists():
            return str(candidate)
    return "powershell.exe"


def create_ffmpeg_zip(zip_path: Path, nested_dir: Path) -> None:
    bin_dir = nested_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    ffmpeg = bin_dir / "ffmpeg.exe"
    ffprobe = bin_dir / "ffprobe.exe"
    ffmpeg.write_text("dummy ffmpeg", encoding="ascii")
    ffprobe.write_text("dummy ffprobe", encoding="ascii")
    with ZipFile(zip_path, "w") as zf:
        zf.write(ffmpeg, ffmpeg.relative_to(zip_path.parent))
        zf.write(ffprobe, ffprobe.relative_to(zip_path.parent))


def run_extraction(zip_path: Path, dest: Path, cwd: Path | None = None) -> None:
    script = (
        "$ErrorActionPreference='Stop';"
        "$ProgressPreference='SilentlyContinue';"
        f"$dl='{zip_path}';"
        f"$dest='{dest}';"
        "if(Test-Path $dest){Remove-Item $dest -Recurse -Force};"
        "Expand-Archive -LiteralPath $dl -DestinationPath $dest -Force;"
        "$ff=Get-ChildItem -Path $dest -Filter 'ffmpeg.exe' -Recurse -File | Select-Object -First 1;"
        "if($ff){Copy-Item $ff.FullName -Destination (Join-Path $dest 'ffmpeg.exe') -Force};"
        "$fp=Get-ChildItem -Path $dest -Filter 'ffprobe.exe' -Recurse -File | Select-Object -First 1;"
        "if($fp){Copy-Item $fp.FullName -Destination (Join-Path $dest 'ffprobe.exe') -Force};"
    )
    subprocess.run(
        [powershell_path(), "-ExecutionPolicy", "Bypass", "-Command", script],
        check=True,
        cwd=str(cwd) if cwd else None,
    )


def main() -> int:
    if platform.system() != "Windows":
        print("[ OK ] Skipping PowerShell extraction test on non-Windows")
        return 0

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        zip_path = tmp / "ffmpeg-release-essentials.zip"
        nested_dir = tmp / "ffmpeg-8.0.1-essentials_build"
        dest = tmp / "FFmpeg"

        create_ffmpeg_zip(zip_path, nested_dir)
        # Run once with a neutral cwd; this should succeed and produce root binaries.
        run_extraction(zip_path, dest)
        root_ffmpeg = dest / "ffmpeg.exe"
        root_ffprobe = dest / "ffprobe.exe"
        if not root_ffmpeg.exists() or not root_ffprobe.exists():
            print("[FAIL] Expected ffmpeg.exe and ffprobe.exe to be copied to install root")
            return 1

        # Running the same script with cwd pinned to the install dir should fail
        # (Remove-Item cannot delete the current working directory). This guards
        # against regressing to the old launcher behavior of starting PowerShell
        # in the target dir.
        try:
            run_extraction(zip_path, dest, cwd=dest)
            print("[FAIL] Extraction unexpectedly succeeded when cwd was the install dir")
            return 1
        except subprocess.CalledProcessError:
            pass

        root_ffmpeg = dest / "ffmpeg.exe"
        root_ffprobe = dest / "ffprobe.exe"
        if not root_ffmpeg.exists() or not root_ffprobe.exists():
            print("[FAIL] Expected ffmpeg.exe and ffprobe.exe to be copied to install root")
            return 1

        content = root_ffmpeg.read_text(encoding="ascii")
        if "dummy ffmpeg" not in content:
            print("[FAIL] ffmpeg.exe content mismatch after extraction")
            return 1

    print("[ OK ] PowerShell extraction copied binaries to install root")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
