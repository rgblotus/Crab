#!/bin/bash
set -e

TOOLS_DIR="/home/neo/.local/crab-tools"
SDK_DIR="/home/neo/.local/crab-sdk"
VULKAN_DIR="/tmp/vulkan-dev"

export PATH="$TOOLS_DIR/usr/bin:$SDK_DIR/usr/bin:/usr/bin:$PATH"
export LD_LIBRARY_PATH="$TOOLS_DIR/usr/lib/x86_64-linux-gnu:$SDK_DIR/usr/lib/x86_64-linux-gnu"
export VCPKG_ROOT="$HOME/vcpkg"
CMAKE_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

BUILD_DIR="build"

echo "=== Configuring ==="
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN" \
    -DCMAKE_BUILD_TYPE=Release \
    -DVulkan_INCLUDE_DIR="$VULKAN_DIR/usr/include" \
    -DVulkan_LIBRARY="/usr/lib/x86_64-linux-gnu/libvulkan.so.1.3.280" \
    -DSDL3_DIR="/home/neo/.local/crab-sdl3/lib/cmake/SDL3" \
    -DCMAKE_BUILD_RPATH="$TOOLS_DIR/usr/lib/x86_64-linux-gnu:$SDK_DIR/usr/lib/x86_64-linux-gnu" \
    -DCMAKE_MAKE_PROGRAM="/usr/bin/make"

echo ""
echo "=== Building ==="
cmake --build "$BUILD_DIR" -j$(nproc)

# Symlink shaders to project root for easy access
ln -sf "$BUILD_DIR/shaders" shaders 2>/dev/null || true

echo ""
echo "=== Populating bin/ ==="
mkdir -p bin
cp "$BUILD_DIR/engine/libCrabEngine.so" bin/
cp "$BUILD_DIR/games/triangle/TriangleGame" bin/
cp -r "$BUILD_DIR/shaders" bin/
chmod +x bin/run.sh

echo ""
echo "=== Build Complete ==="
echo "Run: LD_LIBRARY_PATH=\"build/engine:$TOOLS_DIR/usr/lib/x86_64-linux-gnu:$SDK_DIR/usr/lib/x86_64-linux-gnu\" build/games/triangle/TriangleGame"
echo "   Or: cd bin && ./run.sh"
