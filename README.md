# Crab Engine

A custom C++ game engine built with Vulkan and SDL3, designed for open-world games.

## Architecture

- **CrabEngine** (`engine/`) — shared library (.so) with layered architecture:
  - `Core/` — Logger, types
  - `Platform/` — Window (SDL3), Input
  - `Render/` — VulkanContext, Shader (shaderc), Pipeline, GPUBuffer, RenderMesh, Camera, Renderer
  - `Scene/` — ECS (entt), Components
- **Game** (`games/`) — separate executable linked against CrabEngine
- **Shaders** (`shaders/`) — external GLSL files loaded at runtime

## Dependencies

Managed via vcpkg:
- SDL3 (built from source with Wayland+Vulkan support)
- entt, glm, fmt, shaderc, vulkan-headers, vulkan-loader

System:
- libvulkan (extracted debs)
- Wayland display environment

## Build

```bash
./build.sh
```

This configures CMake, builds the engine + game, and populates `bin/` with:
- `bin/libCrabEngine.so`
- `bin/TriangleGame`
- `bin/shaders/`
- `bin/run.sh`

## Run

```bash
cd bin && ./run.sh
```

Or directly:

```bash
LD_LIBRARY_PATH="build/engine:/home/neo/.local/crab-tools/usr/lib/x86_64-linux-gnu:/home/neo/.local/crab-sdk/usr/lib/x86_64-linux-gnu" build/games/triangle/TriangleGame
```

## VS Code

Three config files in `.vscode/`:
- `tasks.json` — Build, CMake Configure (Debug), Clean
- `launch.json` — Debug/Run TriangleGame (from `bin/`)
- `c_cpp_properties.json` — IntelliSense paths

## Current State

Working: colorful rotating triangle rendered via Vulkan with ECS-driven rotation.
