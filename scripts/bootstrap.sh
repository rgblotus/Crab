#!/usr/bin/env bash
set -euo pipefail

# Configure, build, and test Crab with a CMake preset.
# Usage: ./scripts/bootstrap.sh [preset] [extra cmake args...]
# Example: ./scripts/bootstrap.sh linux-debug
#          ./scripts/bootstrap.sh linux-headless

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

PRESET="${1:-linux-debug}"
if [[ $# -gt 0 ]]; then
  shift
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake was not found. Run ./scripts/setup_ubuntu.sh first." >&2
  exit 1
fi

if ! command -v ninja >/dev/null 2>&1 && ! command -v ninja-build >/dev/null 2>&1; then
  echo "ninja was not found. Run ./scripts/setup_ubuntu.sh first." >&2
  exit 1
fi

echo "==> Configuring ${PRESET}"
cmake --preset "${PRESET}" "$@"

echo "==> Building ${PRESET}"
cmake --build --preset "${PRESET}"

echo "==> Testing ${PRESET}"
ctest --preset "${PRESET}"

echo "==> Running headless smoke test"
"${ROOT_DIR}/build/${PRESET}/bin/crab" --headless --frames 3

echo
echo "Crab is ready: ${ROOT_DIR}/build/${PRESET}/bin/crab"
