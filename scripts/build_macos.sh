#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

command -v brew >/dev/null 2>&1 || {
    echo "Homebrew is required to resolve GLib/Qt dependencies on macOS." >&2
    exit 1
}

BREW_PREFIX="$(brew --prefix)"
LIBFFI_PREFIX="$(brew --prefix libffi 2>/dev/null || true)"

PKG_PATHS=()
if [[ -n "${LIBFFI_PREFIX}" && -d "${LIBFFI_PREFIX}/lib/pkgconfig" ]]; then
    PKG_PATHS+=("${LIBFFI_PREFIX}/lib/pkgconfig")
fi
PKG_PATHS+=(
    "${BREW_PREFIX}/lib/pkgconfig"
    "${BREW_PREFIX}/share/pkgconfig"
)

export PKG_CONFIG_PATH="$(IFS=:; echo "${PKG_PATHS[*]}:${PKG_CONFIG_PATH:-}")"

if ! pkg-config --modversion gio-2.0 >/dev/null 2>&1; then
    echo "pkg-config cannot find gio-2.0. Ensure 'brew install glib libffi' succeeded." >&2
    exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build
