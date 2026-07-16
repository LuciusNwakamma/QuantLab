#!/usr/bin/env bash
# Build QuantLab C++ components
set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"
JOBS=$(nproc 2>/dev/null || echo 4)

echo "==> Configuring ($BUILD_TYPE)..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DQUANTLAB_BUILD_TESTS=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "==> Building with $JOBS parallel jobs..."
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "==> Running C++ tests..."
cd "$BUILD_DIR" && ctest --output-on-failure
