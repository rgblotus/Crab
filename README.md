# Crab

A custom C++20 / Vulkan 1.3 game engine for Linux (Ubuntu 24.04+), built as a
long-term 2D/3D engine and editor. Godot is the UX reference; the architecture
is our own.

**Status:** Phase 0 complete; Phase 2 Vulkan RHI bring-up implemented. The
runtime now creates a Vulkan 1.3 instance/device through volk, vk-bootstrap, and
VMA; renders a textured full-screen triangle with dynamic rendering and
synchronization2; handles swapchain resize/recreation; reads back a headless
offscreen frame; and draws a Dear ImGui debug overlay. Licensed under the MIT
License.

## Stack

- **Platform:** SDL3 (window, input, gamepad, dialogs)
- **Graphics:** Vulkan 1.3, custom RHI, render graph, Forward+ 3D, canvas 2D
- **Core:** EnTT, GLM, enkiTS, spdlog, nlohmann/json
- **Physics:** Jolt (3D), Box2D 3.x (2D)
- **Animation:** ozz-animation + custom state machine
- **Audio:** miniaudio + custom mixer/bus graph
- **Text/UI:** FreeType, HarfBuzz, RmlUi; Dear ImGui for the editor
- **Scripting:** Lua 5.4 + sol2, plus hot-reloadable C++ game modules
- **Profiling:** Tracy (optional, `CRAB_ENABLE_TRACY`)
- **Build:** CMake 3.28+, Ninja, C++20, Clang or GCC

## Quick start

```bash
# Ubuntu 24.04: install the native toolchain and development headers.
./scripts/setup_ubuntu.sh

# Configure, build, test, and run a headless smoke test.
./scripts/bootstrap.sh linux-debug

# Run the windowed runtime.
./build/linux-debug/bin/crab
```

Press `F1` to toggle the debug overlay and `Esc` to quit. Debug builds enable
the Khronos validation layer when it is installed; a validation error is logged
and counted in the overlay (`Validation: on (N errors)`).

Useful commands:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug

./build/linux-debug/bin/crab
./build/linux-debug/bin/crab --headless --frames 5
./build/linux-debug/bin/crab --help
```

On machines without X11/Wayland development packages, use the headless preset:

```bash
cmake --preset linux-headless
cmake --build --preset linux-headless
ctest --preset linux-headless
```

The first configure downloads pinned upstream dependencies with CMake
`FetchContent`. GLSL shaders are compiled to SPIR-V at build time with `glslc`;
the runtime binary is `build/<preset>/bin/crab` and the compiled shaders are
also copied to `build/<preset>/bin/shaders`. See
**[docs/RENDERDOC.md](docs/RENDERDOC.md)** for capturing a frame with RenderDoc
and **[docs/adr/](docs/adr/README.md)** for architecture decisions.

## Project layout

```text
engine/
  include/crab/          Public engine headers
  src/core/              Logging and assertion implementation
  src/platform/          SDL3 platform and window layer
  src/application.cpp    Runtime application lifecycle and main loop
runtime/                 Executable entry point and command-line options
tests/                   Catch2 unit tests and integration smoke tests
cmake/                   CMake modules and generated version header template
third_party/             Pinned dependency declarations (FetchContent)
docs/                    Architecture plan and first-game specification
scripts/                 Ubuntu setup and bootstrap helpers
```

## Development presets

| Preset | Purpose |
|---|---|
| `linux-debug` | Debug build, tests, warnings as errors |
| `linux-release` | Optimized build with tests |
| `linux-asan` | Debug build with AddressSanitizer and UBSan |
| `linux-clang` | Debug build using Clang |
| `linux-headless` | Debug build with SDL desktop video/audio drivers disabled |
| `linux-headless-asan` | Headless build with AddressSanitizer and UBSan |

`CMakePresets.json` also exposes matching build, test, and workflow presets, so
the full pipeline can be run with `cmake --workflow --preset linux-debug`.

See **[CONTRIBUTING.md](CONTRIBUTING.md)** for code style, dependency policy, and
phase discipline.

## Read the plan

The full architecture, phased roadmap, risk register, performance budgets, and
senior review are in **[docs/ENGINE_PLAN.md](docs/ENGINE_PLAN.md)**. Architecture
decisions are recorded in **[docs/adr/](docs/adr/README.md)**.

## First game

A **3D third-person action-adventure vertical slice**: one compact level, a
responsive character, a spring-arm camera, one enemy type with navigation, one
objective, menus, HUD, and 3D audio. See
**[docs/GAME_3D_THIRD_PERSON.md](docs/GAME_3D_THIRD_PERSON.md)** for the game
scope and the selected 3D-first phase order.

## Next milestone

Phase 3: introduce the render graph, a minimal 3D pass (depth buffer, camera,
textured mesh, debug grid), and a minimal 2D batch pass sharing the RHI.

## License

MIT. See **[LICENSE](LICENSE)** and **[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)**.
