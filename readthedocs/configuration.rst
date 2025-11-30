Configuration
=============

External requirements (Windows)
-------------------------------

- Archives are downloaded into the per-app cache at ``%LOCALAPPDATA%\PyAppExec\<app_id>\distrib``. For the shipped FFmpeg requirement, that resolves to ``C:\Users\<you>\AppData\Local\PyAppExec\<app_id>\distrib\ffmpeg-release-essentials.zip``.
- The target install directory is taken from ``requirement_n_install_dir``. In the sample INI this is ``%USERPROFILE%\FFmpeg``.
- After extraction, the launcher looks for the executable name from ``requirement_n_version_check_command`` inside ``install_dir`` (recursing through nested folders such as ``ffmpeg-<version>-essentials_build\bin``). It copies the located binary to the install_dir root and rewrites the version check to that path before re-running it.
- Set ``requirement_n_append_to_path = true`` to prepend the install_dir to ``PATH`` when launching your app.

CLI vs GUI binaries
-------------------

- GUI binaries: ``pyappexec`` and ``pyappexec_installer``. Launch these normally (double-click/Start Menu); flags like ``--help``/``--version`` are only visible when you run them from a terminal.
- CLI binary: ``pyappexec_cli`` (console subsystem). Use this in terminals/automation; it supports ``--help``, ``--version``, and the launcher flags documented above. GUI output is suppressed for ``--help``/``--version`` on the CLI build.
