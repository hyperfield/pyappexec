Troubleshooting
===============

FFmpeg missing on first run
---------------------------

- The FFmpeg archive is downloaded to ``%LOCALAPPDATA%\PyAppExec\<app_id>\distrib``. Confirm the zip exists there.
- Ensure ``requirement_1_install_dir`` points to a writable folder (for example ``%USERPROFILE%\FFmpeg``).
- The launcher now searches ``install_dir`` for ``ffmpeg.exe`` after extraction and copies it to the install_dir root. If the version check still fails, delete the install_dir and rerun to force a fresh extract.

First-run dependency install
----------------------------

- Python packages install only when the managed venv is missing or the requirements file changes. On subsequent runs you should see "Python dependencies already installed; requirements file unchanged."
