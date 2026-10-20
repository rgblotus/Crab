#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format was not found. Run ./scripts/setup_ubuntu.sh first." >&2
  exit 1
fi

find engine runtime tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) \
  -print0 | xargs -0 clang-format --dry-run --Werror

echo "clang-format check passed."
