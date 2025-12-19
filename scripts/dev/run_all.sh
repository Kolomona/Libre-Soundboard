#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/build"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DBUILD_TESTING=ON
cmake --build . -- -j$(nproc)
ctest --output-on-failure

if command -v clang-format >/dev/null 2>&1; then
  cmake --build . --target format || true
fi

echo "Build and tests completed."
