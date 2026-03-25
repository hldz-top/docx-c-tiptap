#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=build
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake configure and build
cmake ..
cmake --build . --config Release

echo "Build finished. Binaries are in $BUILD_DIR"
