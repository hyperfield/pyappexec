#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /path/to/AppName.app" >&2
    exit 1
fi

APP_PATH="$1"
if [[ ! -d "$APP_PATH" ]]; then
    echo "Error: $APP_PATH is not a directory (expected a .app bundle)" >&2
    exit 1
fi

APP_ABS="$(cd "$(dirname "$APP_PATH")" && pwd)/$(basename "$APP_PATH")"
APP_NAME="$(basename "$APP_PATH" .app)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"

mkdir -p "$DIST_DIR"

DMG_PATH="${DIST_DIR}/${APP_NAME}.dmg"

echo "Creating DMG:"
echo "  Source: $APP_ABS"
echo "  Output: $DMG_PATH"

STAGING_DIR="$(mktemp -d -t pyappexec_dmg)"
cleanup() { rm -rf "$STAGING_DIR"; }
trap cleanup EXIT

cp -R "$APP_ABS" "$STAGING_DIR/"
ln -s /Applications "$STAGING_DIR/Applications"

if [[ -f "$DMG_PATH" ]]; then
    rm -f "$DMG_PATH"
fi

hdiutil create -fs HFS+ -volname "$APP_NAME" -srcfolder "$STAGING_DIR" "$DMG_PATH"

echo "DMG created at $DMG_PATH"
