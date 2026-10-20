#!/usr/bin/env bash
set -euo pipefail

# Installs the native dependencies needed to build Crab on Ubuntu 24.04+.
# Run from anywhere: ./scripts/setup_ubuntu.sh

if ! command -v apt-get >/dev/null 2>&1; then
  echo "This script requires apt-get (Ubuntu/Debian)." >&2
  exit 1
fi

if [[ "${EUID}" -ne 0 ]]; then
  SUDO="sudo"
else
  SUDO=""
fi

echo "Installing Crab build dependencies..."
"${SUDO}" apt-get update
"${SUDO}" apt-get install -y --no-install-recommends \
  build-essential \
  ccache \
  clang \
  clang-format \
  clang-tidy \
  cmake \
  git \
  glslc \
  ninja-build \
  pkg-config \
  libasound2-dev \
  libaudio-dev \
  libdbus-1-dev \
  libdecor-0-dev \
  libdrm-dev \
  libegl1-mesa-dev \
  libfribidi-dev \
  libgbm-dev \
  libgl1-mesa-dev \
  libgles2-mesa-dev \
  libibus-1.0-dev \
  libjack-jackd2-dev \
  libpipewire-0.3-dev \
  libpulse-dev \
  libsndio-dev \
  libthai-dev \
  libudev-dev \
  liburing-dev \
  libvulkan-dev \
  libwayland-dev \
  libx11-dev \
  libxcursor-dev \
  libxext-dev \
  libxfixes-dev \
  libxi-dev \
  libxinerama-dev \
  libxkbcommon-dev \
  libxrandr-dev \
  libxss-dev \
  libxtst-dev \
  mesa-vulkan-drivers \
  vulkan-tools \
  vulkan-validationlayers

echo
echo "Done. Configure with:"
echo "  cmake --preset linux-debug"
echo "  cmake --build --preset linux-debug"
echo "  ctest --preset linux-debug"
