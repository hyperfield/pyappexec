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
if [[ -d "${BREW_PREFIX}/opt/zlib/lib/pkgconfig" ]]; then
    PKG_PATHS+=("${BREW_PREFIX}/opt/zlib/lib/pkgconfig")
fi
PKG_PATHS+=(
    "${BREW_PREFIX}/lib/pkgconfig"
    "${BREW_PREFIX}/share/pkgconfig"
)

export PKG_CONFIG_PATH="$(IFS=:; echo "${PKG_PATHS[*]}")"
export PATH="${BREW_PREFIX}/bin:${PATH}"
if [[ -x "${BREW_PREFIX}/bin/pkg-config" ]]; then
    export PKG_CONFIG="${BREW_PREFIX}/bin/pkg-config"
else
    PKG_FALLBACK="$(command -v pkg-config || true)"
    if [[ -z "${PKG_FALLBACK}" ]]; then
        echo "Unable to locate pkg-config. Install it via Homebrew (brew install pkg-config)." >&2
        exit 1
    fi
    export PKG_CONFIG="${PKG_FALLBACK}"
fi

pkgConfigOut="$(${PKG_CONFIG} --modversion gio-2.0 2>&1)"
if grep -q "@@XAMPP_COMMON_ROOTDIR@@" <<< "${pkgConfigOut}"; then
    echo "Detected pkg-config output referencing XAMPP; ensure ${PKG_CONFIG} is first in PATH." >&2
fi

if ! ${PKG_CONFIG} --modversion gio-2.0 >/dev/null 2>&1; then
    echo "pkg-config cannot find gio-2.0. Ensure 'brew install glib libffi' succeeded." >&2
    exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build
