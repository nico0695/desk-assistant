#!/bin/sh
# Renders the UI screens to PNG on the host with the pinned LVGL. No device needed.
set -eu
REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$REPO_ROOT/build-preview"
OUTPUT_DIR="$BUILD_DIR/screens"
LVGL_DIR="$REPO_ROOT/managed_components/lvgl__lvgl"
if [ ! -f "$LVGL_DIR/lvgl.h" ]; then
    echo "LVGL not found in $LVGL_DIR. Run the integrated build once to fetch it." >&2
    exit 1
fi
cmake -S "$REPO_ROOT/tools/preview" -B "$BUILD_DIR/cmake" -G Ninja > /dev/null
cmake --build "$BUILD_DIR/cmake" --target ui_preview page_preview
mkdir -p "$OUTPUT_DIR" "$BUILD_DIR/pages"
"$BUILD_DIR/cmake/ui_preview" "$OUTPUT_DIR"
"$BUILD_DIR/cmake/page_preview" "$BUILD_DIR/pages"
