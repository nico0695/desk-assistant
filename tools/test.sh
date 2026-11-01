#!/bin/sh
# Builds and runs the host tests. Uses IDF_PATH when set, otherwise the documented installation.
set -eu

DEFAULT_IDF_PATH="$HOME/esp/esp-idf-v5.5.2"
REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$REPO_ROOT/build-host"
UNITY_DIR="${IDF_PATH:-$DEFAULT_IDF_PATH}/components/unity/unity/src"

if [ ! -f "$UNITY_DIR/unity.c" ]; then
    echo "Unity not found in $UNITY_DIR. Set IDF_PATH to the ESP-IDF 5.5.2 installation." >&2
    exit 1
fi

cmake -S "$REPO_ROOT/tests" -B "$BUILD_DIR" -G Ninja -D UNITY_DIR="$UNITY_DIR" > /dev/null
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure --no-tests=error
