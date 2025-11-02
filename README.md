# PyAppExec

PyAppExec is a small, cross-platform bootstrapper that prepares a Python application for end users. It locates (or installs) a suitable Python interpreter, provisions an isolated virtual environment, downloads any third-party tooling you bundle, installs Python dependencies, and finally launches your target script — all driven by a simple INI specification.

- [Overview](#overview)
- [Key Features](#key-features)
- [How It Works](#how-it-works)
- [Repository Layout](#repository-layout)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Build](#build)
  - [Run](#run)
- [Configuration](#configuration)
  - [Main section](#main-section)
  - [Requirements section](#requirements-section)
  - [Example](#example)
- [Bundling External Artifacts](#bundling-external-artifacts)
- [Sample Application](#sample-application)
- [Development Notes](#development-notes)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgements](#acknowledgements)

## Overview

Many desktop Python applications require users to install Python, set up virtual environments, download extra binaries such as FFmpeg, and install Python packages before they can run the app. PyAppExec automates those steps so you can distribute a native launcher alongside your Python project. Ship the PyAppExec binary, ship an INI configuration that describes what the launcher should do, and PyAppExec takes care of the rest.

## Key Features

- Detects the highest priority Python interpreter on the PATH and validates its version.
- On Linux, optionally installs Python via `apt`, `dnf`, or `pacman` if a suitable interpreter is not found.
- Creates or reuses a per-project virtual environment and keeps a lightweight state file to avoid redundant `pip install` runs.
- Installs Python package dependencies from a configurable requirements file inside the virtual environment.
- Downloads and, when possible, installs external tools such as FFmpeg, using integrity checks to avoid re-downloading unchanged files.
- Launches the target Python entry point after the environment is ready, with stdout/stderr feedback in the terminal.
- Embeds helper Python scripts via GLib resources so the launcher has zero runtime script dependencies.

## How It Works

1. PyAppExec reads `pyappexec.ini` and selects the `[<OS>:main]` and `[<OS>:requirements]` sections that match the current platform (`Linux`, `MacOS`, `Windows`).
2. It resolves relative paths against the directory that contains the INI file.
3. If no suitable Python interpreter is found, PyAppExec can attempt an installation via common package managers (Linux only) or fall back to the configured download URL so you can prompt users to install it manually.
4. A virtual environment is created (or reused) at the configured location.
5. Python dependencies from `requirements.txt` (or whichever file you point to) are installed inside that environment. A signature file prevents redundant installs when the requirements file has not changed.
6. For each external requirement defined in the INI file, PyAppExec checks whether it is already present and satisfies the minimum version. Missing dependencies are downloaded to the `distrib/` directory or installed via a custom command.
7. Finally, the target Python script is invoked using the virtual environment's interpreter.

## Repository Layout

```
.
├── CMakeLists.txt          # CMake build configuration for the C++ launcher
├── include/                # Public headers for the launcher
├── lib/                    # Implementation sources
├── resources/              # GLib resource manifest embedding helper scripts
├── scripts/                # Embedded helper Python scripts
├── pyappexec.ini           # Example configuration
```

## Getting Started

### Prerequisites

To build the launcher you need:

- A C++20 toolchain (GCC 11+, Clang 13+, or MSVC 19.30+).
- CMake 3.16 or newer.
- `pkg-config` and the development headers for `gio-2.0` (part of GLib) — these provide `glib-compile-resources` and the GIO runtime used to embed scripts.
- Boost (Boost.Process header is required; the default compiled Boost libraries are optional on most platforms).
- The [inih](https://github.com/benhoyt/inih) library with the `INIReader` interface available to CMake as `INIReader` (install system-wide or add it as a submodule and expose the target).
- `curl` (Linux/macOS) or the Windows URLMon APIs (already part of Win32) for downloading requirement archives.

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build step generates `resources.c` from `resources/resources.xml` using `glib-compile-resources`. Ensure that tool is discoverable on your `PATH`.

### Run

After building, run the launcher from the project root so it can find `pyappexec.ini`:

```bash
./build/pyappexec
```

The launcher uses the INI file next to the executable (or the current working directory) to decide what to do. You can ship multiple INI files if you want different app profiles; point PyAppExec at the desired one by copying or symlinking it alongside the binary.

## Configuration

PyAppExec is driven entirely by `pyappexec.ini`. Each operating system gets its own pair of sections: `[Linux:main]` and `[Linux:requirements]`, `[Windows:main]` and `[Windows:requirements]`, and so on.

### Main section

| Key | Required | Description |
| --- | --- | --- |
| `python_download_url` | no | URL you can surface to users if you need to link to an installer. The launcher does not download Python automatically from this URL but exposes it for logging and UX hooks. |
| `python_min_ver` | yes | Minimum acceptable Python version (for example `3.10`). |
| `python_app_dir` | no | Directory that contains your Python project. Defaults to the INI file's directory. |
| `exec_app_path` | yes | Entry-point script relative to `python_app_dir`, usually your `main.py`. |
| `requirements_file` | no | Relative path to the Python requirements file. If omitted, Python dependency installation is skipped. |
| `virtual_env_dir` | no | Directory to create the virtual environment in (relative to `python_app_dir`). Defaults to `.venv`. |

### Requirements section

Define any number of `requirement_<n>` blocks (numbered sequentially from 1). Each block supports:

| Key | Description |
| --- | --- |
| `requirement_<n>` | Human-friendly name, e.g. `FFmpeg`. |
| `requirement_<n>_url` | Download URL for an installer or archive. The file is stored under `distrib/`. |
| `requirement_<n>_file_name` | Override for the downloaded filename. Defaults to the basename of the URL. |
| `requirement_<n>_version_check_command` | Command used to detect the installed version, such as `ffmpeg -version`. |
| `requirement_<n>_version_regex` | Regular expression with a capture group that extracts the version number. |
| `requirement_<n>_min_version` | Minimum acceptable version; omit to accept any detected version. |
| `requirement_<n>_launch_file` | Name of the executable you expect after extraction (used primarily for logging). |
| `requirement_<n>_capture_stderr` | When `true`, merges stderr into stdout during version detection. |
| `requirement_<n>_cmd_params` | Extra parameters appended when running a Windows installer executable. |
| `requirement_<n>_install_command` | Shell command to run for installing the requirement (Linux/macOS). If provided, PyAppExec skips downloading and executes this command directly. |

PyAppExec keeps per-requirement status in memory to avoid noisy logs when the version check command is run multiple times.

### Example

The repository ships with a Linux configuration that targets the sample app in `test/`:

```ini
[Linux:main]
python_download_url = https://www.python.org/ftp/python/3.13.1/python-3.13.1.exe
python_min_ver = 3.10
python_app_dir = test
exec_app_path = main.py
requirements_file = requirements.txt
virtual_env_dir = .pyappexec-venv

[Linux:requirements]
requirement_1 = FFmpeg
requirement_1_url = https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-amd64-static.tar.xz
requirement_1_file_name = ffmpeg-release-amd64-static.tar.xz
requirement_1_version_check_command = ffmpeg -version
requirement_1_version_regex = ^ffmpeg version ([0-9.]+)
requirement_1_min_version = 4.4.2
requirement_1_launch_file = ffmpeg
requirement_1_capture_stderr = false
requirement_1_install_command = sudo apt-get update && sudo apt-get install -y ffmpeg
```

On Windows, optional `cmd_params` can be supplied to run silent installers.

## Bundling External Artifacts

Place offline installers, archives, or wheel files in the `distrib/` directory. PyAppExec stores downloads here and skips re-downloading when the file already exists with the expected size (checked via HTTP `Content-Length`). You can pre-populate this directory before shipping your package to avoid runtime downloads.

## Sample Application

The `test/` directory contains the open-source YT Channel Downloader project as an integration example. To try the full flow:

1. Build PyAppExec.
2. Ensure `ffmpeg` is installed or let the Linux configuration install it for you.
3. Run `./build/pyappexec` from the repository root. The launcher will set up a virtual environment under `test/.pyappexec-venv`, install Python dependencies from `test/requirements.txt`, and start the PyQt application defined in `test/main.py`.

You can swap out the `test/` directory with your own Python app by updating `pyappexec.ini`.

## Development Notes

- The launcher automatically embeds `scripts/get_python_version.py` using GLib resources; edit `resources/resources.xml` if you need to ship additional helper scripts.
- A build generates `.pyappexec_requirements_state` under the virtual environment directory to cache the requirements signature. Delete it if you need to force a reinstall.
- Boost.Process powers all child process execution. Ensure the runtime has permission to spawn subprocesses and access network resources.

## Troubleshooting

- **Python not detected**: Confirm that a compatible interpreter is available on the `PATH`. On Linux, check the console logs to see if PyAppExec attempted package-manager installation.
- **`glib-compile-resources` missing**: Install the GLib development tools package (`libglib2.0-dev-bin` on Debian/Ubuntu, `glib2` on Arch, `brew install glib` on macOS).
- **`INIReader` target not found**: Install the `inih` library or add it to your workspace and expose an `INIReader` target to CMake.
- **External requirement download fails**: Verify the URL, ensure HTTPS certificates are trusted, and check that the host is reachable in your deployment environment.
- **Virtual environment issues**: Remove the existing virtual environment directory (for example `test/.pyappexec-venv`) and rerun the launcher to force a clean setup.

## Contributing

Issues and pull requests are welcome. Please include reproduction steps and platform details when reporting bugs. If you plan to contribute substantial changes, open a discussion first so we can align on direction.

## License

This project is released under the [MIT License](LICENSE).

## Acknowledgements

- [Boost.Process](https://www.boost.org/doc/libs/release/doc/html/process.html) for portable child process management.
- [GLib/GIO](https://docs.gtk.org/gio/) for resource embedding.
- [inih](https://github.com/benhoyt/inih) for INI parsing.
- The [YT Channel Downloader](https://github.com/hyperfield/yt-channel-downloader) project for the sample Python application included under `test/`.
