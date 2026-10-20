# Crab Engine — Architecture & Phased Delivery Plan

**A custom C++20 / Vulkan 1.3 game engine for Linux (Ubuntu 24.04), inspired by Godot's workflow but not its internals.**

- **Status:** Proposal v1.2 — Phases 0 and 2 implemented
- **Date:** 2026-10-20
- **Target:** Linux x86_64 first (Wayland + X11), portable core
- **Toolchain:** CMake 3.28+, Ninja, Clang/GCC, C++20
- **Graphics:** Vulkan 1.3 minimum (1.4 capable), dynamic rendering, synchronization2
- **Scope:** 3D first (third-person game); 2D renderer/tilemaps/Box2D post-v1; editor, PBR, physics, animation, audio, text/UI, scripting, asset pipeline

---

## Contents

- [0. Executive Summary](#0-executive-summary)
- [1. Goals, Non-Goals, and Assumptions](#1-goals-non-goals-and-assumptions)
- [2. Godot as a Reference](#2-godot-as-a-reference-adopt-and-avoid)
- [3. Architecture at a Glance](#3-architecture-at-a-glance)
- [4. Technology Stack](#4-technology-stack)
- [5. Repository and Module Layout](#5-repository-and-module-layout)
- [6. Core Runtime](#6-core-runtime)
- [7. Rendering Architecture](#7-rendering-architecture)
- [8. Physics](#8-physics)
- [9. Animation](#9-animation)
- [10. Audio](#10-audio)
- [11. Editor](#11-editor)
- [12. Asset Pipeline and Tooling](#12-asset-pipeline-and-tooling)
- [13. Platform Layer](#13-platform-layer)
- [14. Build, CI, and Testing](#14-build-ci-and-testing)
- [15. Phased Roadmap](#15-phased-roadmap)
- [16. Sample Games and Vertical Slices](#16-sample-games-and-vertical-slices)
- [17. Risk Register](#17-risk-register)
- [18. Performance Budgets](#18-performance-budgets)
- [19. Plan Review: A Senior Critique](#19-plan-review-a-senior-critique)
- [20. Definition of Done for v1.0](#20-definition-of-done-for-v10)
- [21. First 10 Working Days](#21-first-10-working-days-concrete)
- [Appendix A. CMake and Presets](#appendix-a-cmake-and-presets-sketch)
- [Appendix B. API Sketches](#appendix-b-api-sketches)
- [Appendix C. Glossary](#appendix-c-glossary)
- [Appendix D. Final Recommendation](#appendix-d-final-recommendation)

---

## 0. Executive Summary

Crab is a **code-first, data-driven 2D/3D engine** with a Godot-like authoring workflow, shipping its 3D third-person path first and adding 2D after v1. It is built as layered C++ libraries around a thin SDL3 platform layer, a Vulkan RHI, a render graph, an EnTT-based world with a scene hierarchy, a GUID-based asset database, Lua gameplay scripting plus hot-reloadable C++ game modules, and an ImGui editor.

The plan deliberately **buys commodity systems** (physics, animation sampling, font shaping, audio backend, UI) and **builds the differentiators** (RHI, render graph, asset pipeline, scene/world, editor, gameplay API).

### Key decisions

| Area | Decision |
|---|---|
| Window/input | SDL3 |
| Graphics API | Vulkan 1.3 only, custom RHI + render graph |
| Scene model | EnTT ECS + explicit hierarchy component, exposed as a tree |
| Scripting | Lua 5.4 + sol2; C++ game modules via stable C ABI |
| Physics | Jolt (3D) for v1; Box2D 3.x (2D) post-v1 |
| Navigation | Recast & Detour navmesh (3D) |
| Animation | ozz-animation (3D) + custom state machine for v1; sprite animation post-v1 |
| Audio | miniaudio backend + custom mixer/bus graph |
| Text | FreeType + HarfBuzz + custom glyph atlas |
| Runtime UI | RmlUi; editor UI is Dear ImGui |
| Assets | GUID + `.meta` sidecars + import cache + JSON scenes |
| Build | CMake presets + FetchContent with pinned versions |
| Editor | Separate executable; play mode launches a separate runtime process |

### Selected first game

The first game is a **3D third-person action-adventure vertical slice**. Its full specification is in **[GAME_3D_THIRD_PERSON.md](GAME_3D_THIRD_PERSON.md)**. It changes priority:

- 3D PBR moves to **selected-track Phase 5**, physics/navigation to **Phase 6**, animation to **Phase 7**, and UI to **Phase 10**.
- The 2D renderer, tilemaps, Box2D, sprite animation, and 2D lights move to **post-v1**.
- The game's vertical slice lands at **selected-track Phase 11**, not generic Phase 14.
- The generic phase table in Section 15 remains the reference; the selected 3D-first order in the game document supersedes it.

### Timeline reality check

| Milestone | Optimistic | Realistic (solo, full-time) |
|---|---:|---:|
| Window + Vulkan triangle + ImGui | 6 weeks | 8–10 weeks |
| First 3D PBR showcase | 5 months | 7–9 months |
| First 3D gameplay prototype | 7 months | 9–12 months |
| Editor MVP | 11 months | 14–18 months |
| v1.0 (packaged game) | 14 months | 18–24 months |

Part-time work multiplies these by roughly 2–3×. The phase estimates later in this document are **planning numbers**, not commitments.

### The honest recommendation

This feature list is a **2–3 year engine project**, not a 6-month one. Vulkan is not the hard part; the hard parts are the asset pipeline, editor, UI/text, animation integration, and the hundreds of small systems that make an engine pleasant to use.

- If the goal is **to ship games**, use Godot + GDExtension and spend your time on gameplay.
- If the goal is **to build an engine** (learning, control, a specific long-term product), proceed — but use the selected 3D third-person game as the customer, cut the v1 scope aggressively, and build the runtime before the editor.

The phased plan is designed so that you have a playable 3D third-person prototype before the editor exists, and a packaged game before you polish the engine.

---

## 1. Goals, Non-Goals, and Assumptions

### 1.1 Goals

1. **One runtime for 2D and 3D.** Shared RHI, assets, scene, physics abstraction, audio, input, and scripting. Separate 2D and 3D renderers.
2. **Real-world 3D rendering.** Physically based materials, real light units, HDR pipeline, IBL, cascaded shadows, sky, fog, color management, tonemapping.
3. **Excellent 2D.** Sprite batching, atlases, tilemaps, pixel-perfect cameras, y-sort, parallax, 2D lights (post-v1), sprite animation.
4. **Godot-like iteration speed.** Hot reload for shaders, assets, Lua, and C++ game modules; play mode in under two seconds; editor that stays out of the way.
5. **Data-driven scenes and prefabs.** Stable GUIDs, human-readable scene files, import settings, dependency tracking.
6. **Complete gameplay foundation.** Input actions, cameras, transform hierarchy, physics, animation state machines, audio buses, UI, save/load, localization hooks.
7. **Linux-first, portable core.** All OS calls behind a platform layer; Windows/macOS later without rewriting the engine.
8. **Debuggable and measurable.** Validation layers, RenderDoc, Tracy, GPU timings, frame statistics, golden-image tests.
9. **A stable C++ API and a documented project format.** The engine is a product for its games, not a pile of demos.

### 1.2 Non-goals for v1

- Consoles, mobile, VR/XR.
- C#, visual scripting, or a custom language (Lua first).
- Networking, rollback, or multiplayer framework.
- Terrain, water, weather, volumetric fog, SSR, ray tracing, GPU-driven open worlds.
- AAA-scale streaming or DOTS-scale ECS.
- Full Godot feature parity.
- A custom shading language (GLSL first).
- Asset store, modding SDK, or marketplace.
- Deterministic cross-platform physics.
- In-process editor play mode (separate process first).
- Nested prefab overrides and a full cutscene timeline.
- A 2D renderer, tilemaps, Box2D, and sprite animation in v1. These are post-v1; the first game is 3D.

These are not rejected forever; they are **post-v1**.

### 1.3 Assumptions

- One experienced developer, or a very small team, with solid C++ and basic graphics knowledge.
- Full-time effort for the estimates; part-time roughly doubles or triples them.
- The first game is a **3D third-person vertical slice**; see [GAME_3D_THIRD_PERSON.md](GAME_3D_THIRD_PERSON.md). It is the engine's customer and the main defense against scope creep.
- Target GPU supports Vulkan 1.3 (Mesa 24+, NVIDIA 535+, AMD recent). Vulkan 1.2-only hardware is unsupported in v1.
- Development happens on Ubuntu 24.04 with a discrete or modern integrated GPU. CI uses Mesa lavapipe.
- Only permissively licensed third-party libraries are used.
- Programmer art is acceptable for samples; no art pipeline team is assumed.

### 1.4 Success criteria for v1.0

- A clean clone builds on Ubuntu 24.04 with a **documented bootstrap script** and standard CMake commands, with no manual dependency hunting.
- The 2D sample runs at 4K/60 on a mid-range GPU (1080p/60 on integrated).
- The 3D sample runs at 1080p/60 on a GTX 1660 / RX 580 class GPU.
- The editor can create a scene, place 2D/3D objects, assign scripts, save, and play it.
- Shader, asset, Lua, and C++ game-module hot reload all work in under 2 seconds.
- A debug run produces **zero Vulkan validation errors**.
- A packaged Linux build runs on a clean Ubuntu 24.04 machine without the SDK installed.
- Unit tests, headless render tests, and at least one full playthrough of a sample game pass.
- The engine and all dependencies have a documented license/attribution file.

---

## 2. Godot as a Reference: Adopt and Avoid

Godot is the right UX reference: its scene tree, inspector, import pipeline, and 2D/3D integration are proven. It is the wrong architecture to copy wholesale: deep node inheritance, monolithic classes, and a custom scripting/shading language are expensive to maintain.

### 2.1 Adopt

| Godot concept | Crab equivalent |
|---|---|
| Scene tree | Entity hierarchy exposed as a tree in the editor |
| Signals | Typed event bus + component events |
| Resource UIDs / `.import` files | Asset GUIDs + `.meta` sidecars + import cache |
| Inspector-driven workflow | Reflection-based inspector (entt::meta) |
| Play in editor | Editor launches a runtime process with a scene snapshot |
| Viewports / SubViewports | Render targets and camera viewports |
| Audio buses + effects | Master → Music/SFX/UI buses with sends and snapshots |
| AnimationPlayer / state machine UX | Animation clips + JSON state machine, graph editor later |
| GDExtension hot reload | C++ game module with stable C ABI; Lua per-script reload |
| TileMap | Chunked tilemap layer + Tiled import |
| Project settings | `project.crab` + `settings/` JSON files |
| ClassDB-style reflection | `entt::meta` component registration + Lua bindings |

### 2.2 Avoid

- **Deep `Node` inheritance.** Use composition: entities plus components. The editor can still show a tree.
- **A custom scripting language in v1.** Lua is faster to embed, document, and debug.
- **A custom shading language in v1.** GLSL compiled offline to SPIR-V is enough; consider Slang later.
- **Editor-first development.** The editor is a client of runtime APIs. Build the runtime and a playable game first.
- **Renderer coupled to scene objects.** Extract a render world (meshes, materials, lights, instances) and render that.
- **Global singletons everywhere.** Pass an explicit `EngineContext`/`World`; singletons only for process-wide services (log, jobs, allocator).
- **Feature parity as a goal.** Build what the first two games need, then generalize.
- **Over-abstracted resource handles.** Start with GUID + refcounted handles + explicit load states; add streaming later.

### 2.3 What Godot gets right that is easy to underestimate

- **Iteration time is the feature.** A 30-second import or a 10-second reload kills productivity.
- **The inspector is the interface.** Every serializable property must be editable without code.
- **Undo/redo is not optional.** Every editor mutation must be a command.
- **Import settings belong to the asset.** They must be versioned and survive file moves.
- **2D is a first-class renderer**, not a camera trick on a 3D scene.
- **The scene file is a contract.** Version it, test round-trips, and migrate it.

---

## 3. Architecture at a Glance

```text
+-----------------------------------------------------------------------+
|  crab_editor (ImGui)          |  crab_runtime (game executable)        |
+-------------------------------+---------------------------------------+
|  gameplay: Lua scripts, C++ game module, components, systems          |
+-----------------------------------------------------------------------+
|  scene/world | assets | animation | audio | physics | text | UI       |
+-----------------------------------------------------------------------+
|  renderer: 3D Forward+ | 2D canvas | UI | post | render graph         |
+-----------------------------------------------------------------------+
|  RHI: Vulkan 1.3 (volk, VMA, vk-bootstrap)                            |
+-----------------------------------------------------------------------+
|  platform: SDL3 window/input/gamepad/dialogs, filesystem, time        |
+-----------------------------------------------------------------------+
|  core: memory, jobs, events, reflection, serialization, log, math     |
+-----------------------------------------------------------------------+
```

**Dependency rule:** dependencies point downward only. The renderer never includes scene headers; it consumes an extracted render world. The runtime never depends on the editor. Core depends only on a small set of low-level libraries (GLM, EnTT, spdlog, enkiTS, nlohmann/json) and the C++ standard library. Same-layer modules may depend on each other only when the edge is explicit and acyclic (e.g., physics → scene, animation → scene, UI → renderer + text).

### 3.1 Frame loop

```text
while (running) {
  platform.poll_events();          // SDL3, input, resize, gamepad
  input.update_actions();
  assets.process_hot_reload();     // main-thread-safe queue
  fixed_step();                    // 60 Hz: physics, fixed gameplay
  variable_step();                 // scripts, animation, audio, transforms
  render_world.extract(world);     // scene -> flat render data
  renderer.render(render_world);   // cull, graph, record, submit, present
  world.apply_deferred();          // create/destroy entities safely
  frame.end();                     // frame allocator reset, stats
}
```

- **Fixed update** runs at a configurable rate (default 60 Hz) with an accumulator, max 4 catch-up steps, and an interpolation alpha for rendering.
- **Variable update** receives the real frame delta clamped to 100 ms.
- **Structural changes** (create/destroy entity, add/remove component) are queued and applied at sync points, never during iteration.
- **Pause and time scale** affect variable and fixed time differently; editor pause freezes both, step advances one fixed tick.

### 3.2 Threading model

| Thread | Owns |
|---|---|
| Main | Platform events, scene mutations, Lua, render submission |
| Job workers | Asset import/decode, mesh/texture processing, culling, Jolt physics, audio decode |
| GPU | Async compute/transfer queues; render work is submitted from main initially |
| Audio device | Real-time mixing callback (no locks, no allocation) |

Start with main-thread rendering and jobified asset/physics work. Add a dedicated render thread only after profiling shows submission is the bottleneck. Lua is main-thread-only; C++ systems can opt into jobs.

### 3.3 Core services

- **Memory:** tracking allocator in debug, frame arena (`std::pmr::monotonic_buffer_resource`), asset heap, pool allocator for components. Do not build a general-purpose allocator zoo.
- **Handles:** generational handles for entities, assets, physics bodies, audio voices.
- **Events:** typed immediate bus (`EventBus::emit<T>`) plus a deferred queue for frame-boundary events; scene signals for editor/runtime communication.
- **Reflection:** `entt::meta` for components and properties; used by serialization, inspector, and (with explicit glue) Lua.
- **Logging:** spdlog with categories (`core`, `rhi`, `assets`, `scene`, `physics`, `audio`, `script`), ring buffer for the editor console, file sink in release.
- **Assertions:** `CRAB_ASSERT` (debug), `CRAB_VERIFY` (always), `CRAB_FATAL` (abort with message/backtrace). Exceptions are enabled for std/library compatibility but are not used for gameplay control flow.
- **Time:** `double` elapsed seconds, `float` frame delta, fixed tick counter, time scale, pause.
- **Config:** `project.crab` (JSON) plus versioned settings files for input, physics layers, audio buses, and render quality.

---

## 4. Technology Stack

Pin exact versions at project start; the versions below are **baselines**, not permanent choices. Prefer maintained libraries with permissive licenses.

| Area | Choice | Why | Alternatives / notes |
|---|---|---|---|
| Window, input, gamepad, dialogs | **SDL3** | One dependency covers Linux windowing, Vulkan surface, input, IME, DPI, gamepads, clipboard, file dialogs | GLFW (less coverage); raw X11/Wayland (too much work) |
| Vulkan loader | **volk** | Loads Vulkan entry points, avoids linking the loader, easy extensions | Link `libvulkan` directly |
| Memory allocator | **VMA** | Correct Vulkan memory types, suballocation, budgets | Custom allocator (not worth it) |
| Device/swapchain bootstrap | **vk-bootstrap** | Removes hundreds of lines of boilerplate; optional | Hand-rolled; drop once stable if desired |
| Math | **GLM** | Mature, header-only, Vulkan clip-space flags | DirectXMath (Windows-only), custom math |
| ECS | **EnTT** | Fast, header-only, registry + meta + events | flecs, custom sparse sets |
| 3D physics | **Jolt** | Modern, fast, robust character controller, jobs-friendly | Bullet (older), PhysX (license/size) |
| 2D physics | **Box2D 3.x** | Proven 2D engine, C API, small | Jolt 2D mode (less mature) |
| 3D navigation | **Recast & Detour** | Navmesh baking, query, agents, path following; zlib | Waypoint graph + steering |
| Animation runtime | **ozz-animation** | Production-quality sampling, blending, IK, compression | Custom sampler (months of work) |
| glTF import | **cgltf** | Small, correct, no heavy dependency graph | Assimp (heavy), tinygltf (C++) |
| Mesh processing | **meshoptimizer** | Vertex cache, overdraw, simplification, quantization | Custom |
| Tangents | **mikktspace** | Reference tangent generation | Custom |
| Images | **stb_image**, **tinyexr** | PNG/JPG/TGA/HDR/EXR with tiny footprint | libpng/libjpeg (more deps) |
| Texture compression | **KTX-Software / basisu** | KTX2 + Basis Universal transcoding to BC/ASTC | Raw RGBA (memory cost) |
| Fonts | **FreeType** | Rasterization, hinting; SDF/MSDF via msdfgen or custom | stb_truetype (limited) |
| Text shaping | **HarfBuzz** | Complex scripts, kerning, ligatures; the text engine splits fallback runs | None serious |
| Line breaking / bidi | **libunibreak**, **SheenBidi** | Unicode line breaking, RTL | ICU (larger but complete) |
| Runtime UI | **RmlUi** | HTML/CSS-like UI, data binding, gamepad navigation, MIT | Custom retained UI (large), Dear ImGui (debug only) |
| Editor UI | **Dear ImGui** (docking) | Fast tooling, widely used | Qt (heavy, separate event loop) |
| Gizmos | **ImGuizmo** | Translate/rotate/scale gizmos | Custom |
| Audio backend | **miniaudio** | Device enumeration, hotplug, low-level callback | SDL audio, PortAudio |
| Audio decode | **dr_wav / dr_mp3 / dr_flac**, **stb_vorbis** | Small, permissive, streaming-friendly | libsndfile (GPL/LGPL), libvorbis |
| Scripting | **Lua 5.4 + sol2** | Fast iteration, tiny, easy embedding, coroutines | AngelScript (typed), Wren, C# |
| JSON | **nlohmann/json** | Ergonomic, header-only, good errors | RapidJSON (faster, less ergonomic) |
| Logging | **spdlog** | Fast, formatted, sinks | custom |
| Jobs | **enkiTS** | Lightweight task scheduler, Jolt integration is straightforward | Taskflow, custom |
| Profiling | **Tracy** | CPU/GPU zones, memory, locks, remote capture | Superluminal (Windows), perf |
| Testing | **Catch2** | Header-friendly, expressive | GoogleTest, doctest |
| File watching | **efsw** | Cross-platform watcher; no inotify-only code | platform-specific watcher |
| Compression/packing | **zstd** | Fast, good ratio, streaming | LZ4 (faster, larger) |
| Shader compilation | **glslang / shaderc** | GLSL → SPIR-V, includes, validation | Slang (later) |
| Shader reflection | **SPIRV-Reflect** | Binding introspection, pipeline layout generation | SPIRV-Cross |
| CLI | **CLI11** | Small, clean option parsing | custom |
| Hashing | **xxHash** | Fast content hashes for import cache | std::hash (not stable) |
| Networking (post-v1) | **GameNetworkingSockets** or **ENet** | Proven transports | Custom sockets |

**Licensing:** the baseline stack is permissive (MIT, BSD-2/3, Apache-2.0, zlib, FreeType FTL, public domain). Verify the exact terms at the pinned version and record them in `THIRD_PARTY_NOTICES.md`; do not rely on this table as legal advice.

**Do not build:** physics, navigation, animation sampling, font shaping, audio decoding, image decoding, or a scripting VM. **Do build:** RHI, render graph, scene/world, asset database, editor, gameplay API, tools.

### 4.1 Dependency policy

- One `third_party/CMakeLists.txt` aggregates dependencies; each is pinned to a tag or commit hash.
- Prefer `FetchContent` for reproducibility; allow a system Vulkan loader/headers and system X11/Wayland development packages.
- Keep patches in `third_party/patches/` with a short README per patch.
- Record every dependency and license in `THIRD_PARTY_NOTICES.md`; run a license check in CI.
- Avoid dependencies that pull in large frameworks or require a specific build system at runtime.
- Do not expose third-party types across the game-module ABI.

### 4.2 Ubuntu 24.04 prerequisites

```bash
sudo apt update
sudo apt install -y \
  build-essential clang lld mold ninja-build cmake ccache git pkg-config \
  libvulkan-dev vulkan-tools vulkan-validationlayers mesa-vulkan-drivers \
  libwayland-dev wayland-protocols libxkbcommon-dev libdecor-0-dev \
  libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev \
  libudev-dev libasound2-dev libpulse-dev \
  glslang-tools glslc spirv-tools
```

SDL3, FreeType, HarfBuzz, and the rest can be built from source with FetchContent. If you prefer the LunarG Vulkan SDK for `glslc`, validation layers, and headers, install it from LunarG's apt repository; it is a tooling convenience, not a runtime requirement.

---

## 5. Repository and Module Layout

```text
Crab/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/                       # helpers, options, packaging
├── docs/
│   ├── ENGINE_PLAN.md
│   ├── adr/                     # architecture decision records
│   ├── style.md
│   └── api/
├── engine/
│   ├── include/crab/            # public headers, grouped by module
│   │   ├── core/                # base, memory, jobs, events, time, log, math
│   │   ├── platform/            # SDL3 window/input/filesystem/dialogs
│   │   ├── rhi/                 # RHI interface + Vulkan backend
│   │   ├── renderer/            # render graph, materials, passes, 3D, 2D, text, UI
│   │   ├── scene/               # world, entities, components, systems, hierarchy
│   │   ├── assets/              # asset DB, importers, loaders, cooking
│   │   ├── physics/             # Jolt + Box2D integration
│   │   ├── navigation/          # Recast/Detour navmesh, agents, path following
│   │   ├── animation/           # ozz integration, state machine, 2D animation
│   │   ├── audio/               # device, mixer, buses, effects, spatialization
│   │   ├── text/                # FreeType/HarfBuzz, atlas, layout
│   │   ├── scripting/           # Lua VM, bindings, hot reload
│   │   └── ui/                  # RmlUi integration
│   └── src/                     # implementation, mirroring include/crab modules
├── editor/                      # ImGui editor executable + panels
├── runtime/                     # game runtime executable
├── game/                        # sample game module (shared library)
├── shaders/                     # GLSL sources + include + manifest
├── third_party/                 # pinned FetchContent declarations
├── tools/                       # import/cook/pack, shader compile, stub generation
├── tests/                       # unit, integration, golden images, fixtures
└── assets/                      # sample projects
```

### 5.1 CMake targets

| Target | Type | Purpose |
|---|---|---|
| `crab_core` | static | Base, memory, jobs, events, math, log, reflection |
| `crab_platform` | static | SDL3 wrapper, input, filesystem |
| `crab_rhi` | static | RHI + Vulkan backend |
| `crab_renderer` | static | Render graph, passes, materials, 3D/2D/text renderers |
| `crab_scene` | static | World, ECS, hierarchy, systems |
| `crab_assets` | static | Asset DB, importers, loaders, cooking |
| `crab_physics` | static | Jolt/Box2D integration |
| `crab_navigation` | static | Recast/Detour navmesh, agents, path following |
| `crab_animation` | static | ozz + state machine + sprite animation |
| `crab_audio` | static | Mixer, buses, effects |
| `crab_scripting` | static | Lua VM + bindings |
| `crab_ui` | static | RmlUi backend |
| `crab_engine` | static | Umbrella target linking the modules in order |
| `crab_runtime` | executable | Loads project, runs game module; output binary is `crab` |
| `crab_editor` | executable | Editor, links `crab_engine` + ImGui |
| `crab_game` | shared | Sample gameplay module, hot-reloadable |
| `crab_tests` | executable | Catch2 tests |

Modules may only include headers from lower layers. Same-layer edges must be explicit and acyclic. `crab_renderer` consumes `RenderWorld` data, not `World`/`Entity`.

### 5.2 ABI boundary

- Engine modules are **static libraries** linked into runtime/editor. This keeps inlining and iteration fast.
- Gameplay code is a **shared library** (`crab_game.so`) loaded by the runtime, with a small **C ABI** (`extern "C"`) and opaque handles.
- Do not pass STL types, exceptions, or engine C++ objects across the module boundary. Pass POD structs, pointers, and `uint64_t` handles.
- The editor compiles the game module by invoking CMake/Ninja and captures compiler output for the console.

---

## 6. Core Runtime

### 6.1 Memory, containers, handles

- `Allocator` interface with `allocate/free/reallocate`; a `TrackingAllocator` in debug reports leaks and tags allocations (rendering, physics, audio, assets).
- `FrameArena` resets each frame for temporary render/cull data.
- `AssetHeap` for long-lived loaded data; budgeted and evicted by the asset system.
- Use `std::vector`, `std::unordered_map`, and `std::pmr` where appropriate. Do not build custom containers unless profiling proves a need.
- Generational handles: `Handle<T>{uint32 index, uint32 generation}`; stale handles fail safely.
- Entity handles are EnTT entities; expose them as `EntityID` (packed `uint64`) to scripts and serialization.

### 6.2 Jobs

- enkiTS scheduler with worker count = physical cores − 1, leaving one core for the main/audio thread.
- `JobCounter`/`JobGroup` for fork-join; `parallel_for` for culling, skinning prep, asset import.
- Jolt integrates through its own `JobSystem` adapter backed by enkiTS.
- The audio callback never allocates, locks, or waits on a job.
- Main thread waits only at explicit sync points (end of frame, asset barrier); avoid hidden waits in render code.
- Start single-threaded for correctness; introduce jobs behind measured hot spots.

### 6.3 Events and signals

- `EventBus` with typed `emit<Event>(event)`; listeners register with a `ListenerHandle` and are removed automatically on destruction.
- Deferred events are queued and flushed once per frame.
- Scene signals (`entity_destroyed`, `component_added`, `scene_loaded`) are engine events, not a separate mechanism.
- Scripts subscribe through `events.on("player_died", fn)`; C++ uses typed events.
- Never mutate the world from inside an iteration; queue a command.

### 6.4 Reflection and serialization

- Register components with `entt::meta`:

```cpp
CRAB_COMPONENT(Transform)
  .property("position", &Transform::position)
  .property("rotation", &Transform::rotation)
  .property("scale",    &Transform::scale);
```

- The same registry powers the inspector, JSON scene serialization, copy/paste, and prefab overrides.
- Serialization is versioned per file and per component type; migrations are small functions, not ad-hoc `if` chains.
- Scene files are JSON (human-readable, diffable, merge-friendly). Binary cooked scenes are a later optimization.
- Every entity and asset has a stable UUID; references are GUIDs, never raw paths.
- Round-trip tests are mandatory: `load(save(scene)) == scene` at the data level.

### 6.5 Scene model: ECS with a tree interface

**Decision:** the world is an `entt::registry`; the editor shows a tree backed by a `Hierarchy` component. This avoids Godot's deep node inheritance while preserving the authoring UX.

Core components:

| Component | Responsibility |
|---|---|
| `Transform` | Local position, rotation, scale; world matrix cache; dirty flag |
| `Hierarchy` | Parent, first child, next/prev sibling; cycle checks |
| `Identity` | UUID, name, active/visible flags, tags, layer |
| `Script` | Lua script asset + per-instance state table |
| `Camera` | Projection, FOV/size, near/far, viewport, priority, culling mask, post profile |
| `MeshRenderer` | Mesh asset, material(s), shadow flags, LOD bias |
| `Sprite` | Texture/atlas region, color, flip, layer/z, material |
| `Tilemap` | Tileset, chunks, layers, collision flags |
| `Light` | Directional/point/spot, color, intensity, range, shadow settings |
| `RigidBody` / `Collider` | 3D/2D body type, shape, mass, friction, layers, events |
| `CharacterController` | Capsule/shape, slope/step, gravity, move API |
| `Animator` | Skeleton, clips, state machine asset, parameters |
| `AudioSource` | Clip, bus, volume, pitch, loop, 2D/3D mode, attenuation |
| `AudioListener` | One active listener per viewport/world |
| `UIView` | RmlUi document rendered to screen/world |
| `ParticleEmitter` | CPU/GPU emitter settings |

**Systems** run in a fixed order:

```text
input -> scripts(variable) -> fixed gameplay -> physics -> animation
      -> hierarchy/transform -> culling -> render extract -> audio -> UI
```

- Systems are plain functions/classes operating on queries; they are registered in a schedule with explicit ordering.
- Structural changes go through `CommandBuffer` (`create`, `destroy`, `add<T>`, `remove<T>`) applied at sync points.
- Prefabs are serialized entity subtrees; instantiation remaps UUIDs and asset references. Nested prefab overrides are post-v1.
- Scene instancing supports additive loading, persistent scenes, and async transitions.

### 6.6 Units, coordinates, and large worlds

- **Right-handed, Y-up, −Z forward**, matching Godot/glTF conventions.
- **1 unit = 1 meter, 1 kilogram, 1 second.** Physics gravity is −9.81 m/s². Light intensities use physical units (lux/candela) with an exposure model.
- Use `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `GLM_FORCE_RADIANS`, and a Vulkan-correct projection (Y flip or negative-height viewport). Prefer **reverse-Z with an infinite far plane** for 3D depth.
- CPU transforms are `float`; a `WorldOrigin` is `double`. Above ~4 km from the origin, rebase the origin and shift physics/render transforms camera-relative. Full large-world streaming is post-v1.

### 6.7 Asset database

- Each source asset has a `.meta` sidecar containing its GUID, importer type, import settings, and dependencies.
- Import pipeline: `source -> importer -> intermediate binary -> runtime loader`. The cache key is `hash(source + settings + importer version)`.
- Moving/renaming a source keeps its GUID because the `.meta` moves with it. Deleting a `.meta` is a deliberate "new asset" operation.
- Loads are asynchronous and return `AssetHandle<T>`; states are `Unloaded -> Loading -> Ready -> Failed`.
- The dependency graph prevents unloading in-use assets and drives hot reload.
- Dev builds read loose files and watch for changes; release builds cook into a `.crabpak` (zstd, memory-mapped, table of contents).
- Missing/broken assets produce a visible placeholder (magenta checker, error text), never a crash.

### 6.8 Scripting

- Lua 5.4 + sol2. Each script is a module returning a table with lifecycle functions:

```lua
local M = {}

function M:on_ready()
  self.speed = 4.0
end

function M:on_update(dt)
  local t = self.entity:get_transform()
  t.position = t.position + vec3(0, 0, -self.speed * dt)
end

return M
```

- Lifecycle: `on_ready`, `on_update(dt)`, `on_physics_update(fixed_dt)`, `on_event(name, data)`, `on_destroy`.
- Coroutines: `wait(seconds)`, `wait_frames(n)`, `tween(entity, property, target, duration)`.
- Sandbox: no `io`, `os.execute`, or package loading by default; expose only the engine API and a project allowlist.
- Hot reload reloads a script and re-runs `on_ready`; persistent state must be serialized or stored in an explicit state table.
- C++ gameplay uses the game module ABI for performance-critical systems; Lua handles glue, UI, and content logic.
- Generate EmmyLua stubs from the binding registry so the editor/IDE has autocomplete.
- Lua runs on the main thread only. Long tasks (pathfinding, asset work) go to C++ jobs.

---

## 7. Rendering Architecture

### 7.1 RHI (Render Hardware Interface)

The RHI is a thin Vulkan abstraction, not a cross-API abstraction. It owns device objects and exposes explicit, low-level operations so the renderer can use modern Vulkan without leaking Vulkan types everywhere.

```cpp
namespace crab::rhi {

struct DeviceDesc { bool validation; bool preferDiscrete; };
struct FrameStats { double cpu_ms, gpu_ms; uint32 draw_calls, triangles; };

class Device {
public:
  static std::unique_ptr<Device> create(const DeviceDesc&, platform::Window&);
  virtual ~Device() = default;

  virtual Frame& begin_frame() = 0;
  virtual void  end_frame(Frame&) = 0;
  virtual BufferHandle  create_buffer(const BufferDesc&) = 0;
  virtual TextureHandle create_texture(const TextureDesc&) = 0;
  virtual PipelineHandle create_graphics_pipeline(const GraphicsPipelineDesc&) = 0;
  virtual void  destroy(BufferHandle) = 0;
  // ...
};

} // namespace crab::rhi
```

**Vulkan baseline:**

- Vulkan 1.3 core (dynamic rendering, synchronization2) plus timeline semaphores (Vulkan 1.2).
- `VK_KHR_swapchain`, `VK_KHR_surface`; `VK_EXT_debug_utils` in debug.
- VMA for all allocations; staging ring buffer for uploads; per-frame transient images.
- 2–3 frames in flight; acquire/present semaphores; swapchain recreation on resize/minimize.
- Descriptor indexing (bindless) as a **tier**: design the material/binding API for it, but ship the basic per-material/per-frame path first. When enabled, one global set holds sampled textures/samplers plus a per-frame uniform ring; otherwise use per-material sets.
- Pipeline cache persisted to `.crab/cache/pipelines.bin`, keyed by driver/engine version.
- GPU timestamp queries and `VK_EXT_memory_budget` for stats.
- Validation layers, best-practice warnings, and debug labels in debug builds. Zero validation errors is a merge gate.
- Prefer FIFO present; MAILBOX optional. Frame pacing and latency controls later.

**Not in v1:** ray tracing, mesh shaders, shader objects, descriptor buffers, sparse resources, multi-GPU, HDR display output.

### 7.2 Render graph

A frame is described as passes with declared reads/writes; the graph derives barriers, layouts, and transient-resource lifetimes.

```cpp
graph.add_pass("gbuffer")
  .color("albedo", RGBA8)
  .color("normal", RGBA16F)
  .depth("depth", D32F)
  .execute([&](PassContext& ctx) { /* ... */ });

graph.add_pass("lighting")
  .read("albedo").read("normal").read("depth")
  .write("hdr", RGBA16F)
  .execute(...);
```

- Start with a **linear graph** and automatic barriers; add aliasing, async compute, and multi-queue scheduling only when needed.
- Transient resources are pooled per frame; persistent resources (history buffers, shadow maps, IBL) are explicit.
- Passes can render to the swapchain or to textures (for viewports, UI, and post effects).
- The graph is the single place that knows about resource states; renderer code never calls `vkCmdPipelineBarrier` directly.
- Debug: dump the graph as text/JSON, visualize passes, name resources with `VK_EXT_debug_utils`.

### 7.3 Shaders and materials

- GLSL 460+ compiled offline to SPIR-V with `glslc` (or `shaderc`); `#include` supported.
- A shader manifest records stage, entry point, variants, and reflected bindings (SPIRV-Reflect); the RHI builds pipeline layouts from reflection.
- Shader variants use `#define` permutations with a compile cache; avoid combinatorial explosions.
- Dev hot reload: watch `.vert/.frag/.comp`, recompile, recreate pipelines, keep the frame running.
- Release: compile at build time and pack SPIR-V; optional runtime shader cache.
- Material model: metallic-roughness PBR; alpha modes (opaque, mask, blend); double-sided; normal/AO/emissive; per-material uniforms in a storage buffer; textures via bindless where supported, per-material bindings otherwise.
- Material instances override parameters without duplicating shaders.
- Compile errors are surfaced in the editor console with file/line and a visible fallback material.

### 7.4 Color pipeline

- Work in **linear** color; `RGBA16F` HDR scene target.
- sRGB textures are decoded on sample; sRGB swapchain encodes on present.
- Exposure in EV100 with manual and automatic modes; tonemapping (ACES or AgX); optional color-grading LUT.
- 2D UI and pixel-art content can bypass tonemapping via an explicit LDR canvas path; document the choice per render layer.
- Consistent white point and gamma; test with a color chart and gradient ramps.

### 7.5 3D renderer

**Pipeline:** clustered Forward+ with a depth prepass optional, HDR target, MSAA optional, post chain.

| Feature | v1 | Notes |
|---|---|---|
| PBR metallic-roughness | Yes | GGX/Smith/Schlick, energy conservation |
| Directional + point + spot lights | Yes | Clustered light grid (compute), max ~256 lights |
| Cascaded shadow maps | Yes | 4 cascades, stable snapping, PCF |
| IBL | Yes | Irradiance SH + prefiltered specular + BRDF LUT |
| Sky | Yes | Procedural gradient/physical sky, sun disk, ambient |
| Fog | Yes | Exponential/height fog |
| Transparency | Yes | Sorted, alpha blend; OIT later |
| Skinning | Yes | GPU skinning, 4 influences, bone palette storage buffer |
| Instancing | Yes | Per-instance transforms; indirect later |
| Post | Yes | Bloom, exposure, tonemap, FXAA; TAA after motion vectors |
| SSAO | Optional | After core lighting |
| SSR / volumetrics / decals / terrain / water | No | Post-v1 |
| GPU-driven culling | No | CPU frustum culling + jobs first |

**Mesh format:** positions, normals, tangents, UV0/UV1, vertex color, joints/weights; 16/32-bit indices; submeshes with material slots; bounding sphere/AABB; optional LODs.

**Culling:** frustum + distance + layer mask on worker jobs; build a flat instance list for the renderer. Do not traverse the ECS per draw.

**Render extraction:** the scene produces `RenderWorld { meshes[], materials[], instances[], lights[], cameras[], particles[] }` with stable handles. This keeps the renderer independent and makes tests easy.

**Anti-aliasing:** start with FXAA; add TAA only after motion vectors, jitter, and history rejection are correct. TAA is a project, not a checkbox.

**Resolution:** internal render scale (0.5–2.0), optional FSR later; UI renders at native resolution.

### 7.6 2D renderer

2D is a separate renderer sharing the RHI, not a 3D scene with an orthographic camera.

- **Canvas layers** composited in order; each layer has a camera, blend mode, and optional post flags.
- **Sprite batching:** vertex format (position, UV, color, normal, material ID); batch by texture/atlas/material/blend; index buffer; dynamic vertex ring.
- **Sorting:** layer → z-index → y-sort (for top-down games); stable order for determinism.
- **Atlases:** runtime atlas packing, trim, padding, extrude; support multiple pages.
- **Pixel-perfect:** camera pixel snapping, integer zoom, nearest filtering option, resolution-independent UI; test at 1080p/1440p/4K.
- **Tilemaps:** chunked grids, tilesets, autotile rules, collision shapes, Tiled (TMX/TSX) import; editor painting later.
- **2D lights (post-v1):** normal maps, light accumulation target, occluders/SDF shadows, blend modes.
- **Parallax:** scroll factors per layer, infinite repeat.
- **Particles:** CPU billboards/quads first; GPU later.
- **Camera2D:** limits, smoothing, deadzone, zoom, shake, pixel snap, multiple viewports.
- **Text:** see below; world-space and screen-space text share the shaping/atlas stack.

### 7.7 Text stack

Text is one of the most underestimated systems. Budget for it.

- **Shaping:** HarfBuzz (kerning, ligatures, complex scripts, fallback runs).
- **Rasterization:** FreeType; glyph cache keyed by face, size, DPI, subpixel mode.
- **Atlas:** dynamic pages (RGBA8), LRU eviction, padding to avoid bleeding; MSDF/SDF optional for scalable world text.
- **Layout:** line breaking (libunibreak), bidi (SheenBidi), alignment, wrapping, ellipsis, rich-text spans, inline images later.
- **Font fallback:** ordered family list; per-glyph fallback; CJK/Arabic/Devanagari support depends on fonts.
- **DPI/scale:** layout in logical units; rasterize at physical size; re-rasterize on scale change.
- **World text:** billboard/3D quads with SDF or screen-space sizing; occlusion optional.
- **Editor/debug text:** ImGui's built-in font atlas; do not use it for game UI.
- **RmlUi:** start with its FreeType font engine; plug our HarfBuzz shaping later if complex scripts are required.

### 7.8 Runtime UI

- **RmlUi** for game UI: HTML/CSS-like documents, data binding, templates, animations, focus, keyboard/gamepad navigation.
- Render interface maps RmlUi draws to the 2D/UI pass; input interface maps SDL3 events; system interface for time and clipboard.
- Screen-space UI by default; world-space UI via a render target/quad.
- DPI-aware scaling and safe-area insets.
- Editor UI is **Dear ImGui** (docking, console, inspector, profiler); keep the two stacks separate.
- Accessibility (screen readers, high contrast) is post-v1, but keep semantic roles in the UI data model.

### 7.9 Cameras

A `Camera` component supports:

- Perspective/orthographic, FOV or ortho size, near/far, aspect, viewport rect, priority, culling mask, clear mode, HDR/post profile.
- Multiple active cameras → multiple viewports/split-screen; render-to-texture for portals/security cameras.
- Controllers: fly, orbit, first-person, third-person spring arm with collision, 2D follow with deadzone/limits, cinematic path (post-v1).
- Camera shake, FOV kick, smoothing, and physics interpolation.
- Camera-relative rendering for large worlds; jitter for TAA; exposure adaptation.
- Editor camera is separate and never serialized into the scene.

### 7.10 Rendering performance rules

- Build a flat render world; no ECS queries per draw.
- Batch/instance by pipeline and material; target <1,500 draw calls for the 3D sample.
- Use a depth prepass only if measured; it is not automatically faster.
- Keep the number of render targets and full-screen passes small; measure bandwidth.
- Use clamped frame delta, maximum frames in flight, and a frame-time graph in the editor.
- Every feature has a quality toggle and a measured cost; no "it looks better" merges.

---

## 8. Physics

### 8.1 3D: Jolt

- One `PhysicsSystem` per world; body interface, layers, broadphase, and a job-system adapter backed by enkiTS.
- Shapes: box, sphere, capsule, cylinder, convex hull, triangle mesh, heightfield (post-v1), compound.
- Body types: static, kinematic, dynamic; motion quality; CCD for fast bodies; sleeping.
- Character controller via `CharacterVirtual`: slope limit, step height, snap-to-ground, moving platforms.
- Collision events: enter/stay/exit, sensors/triggers, contact points; delivered through the event bus.
- Queries: raycast, shape cast, overlap; layer/mask filtering; ignore-self.
- Joints: fixed, hinge, slider, distance, cone-twist, six-DOF; breakable later.
- Physics materials (friction, restitution) as assets.
- Sync: ECS `Transform` ↔ body; write back after step; interpolate for rendering; teleport API resets velocity.
- Debug draw: colliders, contacts, broadphase, constraints; editor toggle.

### 8.2 2D: Box2D 3.x

- One world per 2D scene; fixed step; bodies and shapes via the C API.
- Shapes: box, circle, capsule, polygon, chain/edge.
- Joints: distance, revolute, prismatic, weld, wheel.
- Sensors, contact events, ray/shape queries.
- Character controller: custom kinematic controller on top of Box2D (slope/step/ground checks).
- Same layers, collision matrix, and debug-draw interface as 3D where possible.

### 8.3 Fixed step and interpolation

- Default 60 Hz; configurable. Accumulator with max 4 substeps; if the frame is too slow, drop time rather than spiral.
- Physics runs before animation/gameplay that depends on it; render interpolates previous/current transforms by alpha.
- Pause/step in the editor is deterministic at the fixed rate.
- Do not claim cross-platform determinism. Same-binary/same-platform replay is a later goal.

### 8.4 Navigation (3D)

A 3D third-person game with enemies needs navigation. This is an engine module, not game-specific glue.

- **Recast & Detour** (zlib): bake navmesh from level geometry, query paths, follow corridors.
- Bake at import/build time from a dedicated collision/geometry set; store the navmesh as an asset with the level.
- Agent component: radius, height, max speed, acceleration, avoidance, area costs.
- Queries: nearest point, random point, raycast, path corridor; path following with steering.
- Debug draw: navmesh polygons, agent paths, areas, off-mesh links.
- Off-mesh links (ladders, jumps) are post-slice.
- Fallback if the schedule slips: waypoint graph + steering + obstacle avoidance. It saves 2–3 weeks but produces limited enemies.

---

## 9. Animation

### 9.1 3D skeletal animation

- Import glTF skins, joints, inverse bind matrices, and animation clips with cgltf.
- Runtime: **ozz-animation** skeleton, sampling, blending, layers, and IK; optional ozz offline compression.
- Retargeting: ozz offline tools or rest-pose remapping; validate skeleton compatibility early (Mixamo-style rigs are a practical source).
- Clips have duration, loop mode, events/markers, root-motion flag, and speed.
- Blend modes: override, additive, blend; per-bone masks; sync groups for walk/run cycles.
- State machine asset (JSON):
  - states (clip or blend tree), parameters (float/bool/trigger), transitions with conditions, exit time, duration, priority.
  - 1D/2D blend spaces (speed, direction).
  - layers with masks and weights; events forwarded to gameplay/audio.
- Root motion: extract translation/rotation, apply to the entity or a character controller; toggle per clip.
- IK: two-bone, aim, foot placement; full-body IK post-v1.
- Skinning: GPU; bone palette in a storage buffer; 4 influences; normals/tangents skinned.
- Morph targets: post-v1.

### 9.2 2D animation

- Sprite-sheet animation: frames, FPS, loop/ping-pong, events per frame.
- Animated sprite component with flip, tint, material, and z-order.
- Aseprite/TexturePacker JSON import; atlas regions.
- 2D skeletal animation (Spine/DragonBones) is post-v1; do not build a runtime for it in v1.

---

## 10. Audio

### 10.1 Backend and mixer

- **miniaudio** device (48 kHz, f32, ~10 ms period), hotplug and default-device changes.
- Custom mixer graph: voices → buses → master; sends for reverb/delay; limiter on master.
- Bus layout is a project asset: Master, Music, SFX, UI, Voice, Ambient; volume, mute, solo, effects, sends.
- Voice management: priority, virtual voices, max concurrent, fade-out stealing.
- Streaming for music/long ambiences; fully decoded one-shots pooled by asset.
- Decoders: WAV, MP3, FLAC, Ogg Vorbis; decode on job threads into ring buffers.
- Resampling and channel mapping at load; no allocation in the audio callback.

### 10.2 Spatial audio

- Listener component tied to the active camera; position/orientation updated once per frame.
- 3D attenuation (inverse/linear/exponential), panning, distance filtering, Doppler (optional), air absorption (optional).
- Occlusion/obstruction via raycasts (post-v1); reverb zones (post-v1); HRTF (post-v1).
- 2D sources: volume, pan, pitch; UI bus is always non-spatial.
- Snapshots: menu/pause/underwater presets with crossfade; ducking (music under dialogue).

### 10.3 Import and tooling

- Per-asset settings: loop, trim, normalize, mono/stereo, quality, preload/stream.
- Waveform preview and audition in the editor (later).
- Missing audio asset → silent placeholder with a log error, never a crash.

**Timebox note:** if the custom mixer/effects work exceeds ~3 weeks, evaluate **SoLoud** as a drop-in for buses/effects/spatialization. Keep the engine-facing API stable so the backend can change.

---

## 11. Editor

The editor is a **client of the runtime APIs**, not the core of the engine. Build a minimal debug overlay early, but do not build the full editor until the runtime can run a game.

### 11.1 MVP panels

| Panel | Contents |
|---|---|
| Viewport | Rendered scene, camera navigation, grid, selection outline, gizmos, play controls |
| Hierarchy | Entity tree, drag/drop reparenting, search, visibility/active toggles |
| Inspector | Reflection-driven properties, asset pickers, component add/remove, multi-edit |
| Asset Browser | Project tree, thumbnails, import settings, drag/drop, rename/move, reimport |
| Console | Log categories, filters, search, script errors, compile output |
| Stats | Frame CPU/GPU, draw calls, triangles, memory, asset loads, job utilization |
| Project Settings | Input map, physics layers, audio buses, render quality, tags |
| Animation | Clip preview, state machine, parameters, events (phase 2) |
| Tilemap | Tile palette, paint/erase, collision layers (phase 2) |

### 11.2 Editor architecture

- Dear ImGui docking + ImGuizmo; viewport rendered to a texture.
- **Undo/redo:** every mutation is a command with `do`/`undo`; inspector edits are transactions; asset moves are commands.
- **Selection:** editor-only concept; multi-select supported; selection syncs across hierarchy/viewport/inspector.
- **Play mode:** save a temporary scene snapshot, launch `crab_runtime --project ... --scene snapshot --play`, stream logs back, stop kills the process. In-process play is post-v1.
- **Hot reload:** shaders, assets, Lua; C++ module rebuild and reload from the editor.
- **Build:** invoke CMake/Ninja, show errors, run the packaged game.
- **Settings:** persist editor layout, recent projects, camera positions, and preferences in XDG config.
- **Gizmos:** translate/rotate/scale, snapping, local/world space, grid, camera preview.
- **Picking:** ray vs collider/AABB first; GPU ID buffer later.
- **Prefabs:** create, instantiate, edit, and apply (basic overrides) after the MVP.
- **Testing:** editor smoke tests via ImGui test engine (later); manual checklist for every panel.

### 11.3 What "editor MVP" means

You can: create a project, import assets, create a scene, add 2D/3D entities, edit transforms/materials/colliders/scripts, save, play, inspect logs, and exit without corruption. That is 6–8 weeks after the runtime is stable. A Godot-class editor is 6–12 months more.

---

## 12. Asset Pipeline and Tooling

### 12.1 Importers

| Source | Output | Notes |
|---|---|---|
| glTF/GLB | Mesh, material, skeleton, animation, scene | cgltf; axis/scale conversion; meshoptimizer |
| PNG/JPG/TGA/HDR/EXR | Texture (KTX2/BC/ASTC/RGBA) | sRGB/linear, mips, compression, max size |
| WAV/MP3/FLAC/OGG | Audio clip | loop/trim/normalize/stream settings |
| TTF/OTF | Font | glyph cache is runtime; import validates |
| GLSL | SPIR-V + reflection | variants, includes, hot reload |
| JSON scene | Cooked scene | versioned, GUID refs |
| Tiled TMX/TSX | Tilemap/tileset | tile layers, collision, properties |
| Aseprite/atlas JSON | Sprite frames | atlas regions, animation clips |

### 12.2 Intermediate formats and cooking

- Intermediate files are versioned binary formats with a magic number, version, and endianness; loaders reject mismatches with a clear error.
- The import cache lives in `.crab/cache/` (not committed). Source assets and `.meta` files are committed.
- Cook step: strip editor-only data, compress textures, compile shaders, pack assets, build dependency table.
- `.crabpak`: header + TOC + zstd-compressed blocks; memory-mappable; per-asset offsets; optional encryption later.
- Content hashing (xxHash) for cache keys and integrity; never use timestamps as the only invalidation signal.
- Import runs in parallel jobs with progress and cancellation.

### 12.3 Scene and project format

```json
{
  "format": "crab.scene",
  "version": 1,
  "entities": [
    {
      "id": "5b0f...",
      "name": "Player",
      "parent": null,
      "components": {
        "Transform": { "position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1] },
        "MeshRenderer": { "mesh": "guid://...", "material": "guid://..." },
        "Script": { "script": "guid://..." }
      }
    }
  ]
}
```

- Stable ordering for diffs; arrays instead of maps where order matters.
- References are GUIDs; paths are resolved through the asset database.
- Versioned migrations run on load; a scene saved by a newer version fails loudly rather than silently corrupting.
- `project.crab` defines name, version, entry scene, asset roots, and settings paths.

---

## 13. Platform Layer

- **SDL3** owns window creation, Vulkan surface, event loop, keyboard/mouse, gamepad, haptics, text input/IME, clipboard, drag-and-drop, file dialogs, message boxes, display/DPI, and power events.
- **Wayland and X11** are both supported; test both. Watch fractional scaling, IME popups, and cursor locking on Wayland.
- **Input:** raw device state → action map (`move_forward`, `jump`, `fire`) → gameplay. Supports keyboard, mouse, gamepad, deadzones, rebinding, and multiple bindings per action.
- **Filesystem:** project paths are UTF-8; use `std::filesystem`; normalize separators; warn about case-sensitivity assumptions. Config/cache/logs go to XDG directories.
- **Time:** monotonic clock; sleep/spin hybrid for frame pacing later; no `std::chrono` in hot loops if it shows up in profiles.
- **Crash handling:** signal handlers with backtrace, log flush, and a crash dump path; Crashpad/minidump integration later.
- **Threads:** platform wrapper over `std::thread`; thread naming for profilers.
- **Locale:** UTF-8 everywhere; localization assets are JSON/PO with ICU-ready hooks; RTL layout later.
- **Single instance / file associations:** post-v1.

---

## 14. Build, CI, and Testing

### 14.1 CMake

- CMake 3.28+, Ninja, C++20, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`.
- Presets: `linux-debug`, `linux-release`, `linux-asan`, `linux-tsan`, `linux-headless`.
- Compiler flags: `-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor`; warnings-as-errors in CI; no `-march=native` for distributed builds.
- Link with mold or lld; ccache enabled; unity builds off (better errors and incremental behavior).
- Debug: `-O1 -g -fno-omit-frame-pointer`; Release: `-O2 -g` (keep symbols) + LTO later.
- Options: `CRAB_BUILD_EDITOR`, `CRAB_BUILD_TESTS`, `CRAB_ENABLE_VALIDATION`, `CRAB_USE_SYSTEM_VULKAN`.
- Shaders compiled by custom commands into `build/shaders/`; assets copied/symlinked in dev.
- Install/export rules and `$ORIGIN` RPATH for packaged Linux builds.

### 14.2 Dependencies

- FetchContent with pinned tags/commits; optional `CRAB_DEPS_OFFLINE` for reproducible builds.
- System packages only for Vulkan loader/headers and X11/Wayland development files.
- Every patch documented; every license in `THIRD_PARTY_NOTICES.md`.
- Renovate/Dependabot-style quarterly dependency review; do not update mid-milestone.

### 14.3 Testing strategy

| Level | What | Tooling |
|---|---|---|
| Unit | Math, handles, events, serialization round-trip, asset DB, animation sampling, job system | Catch2 |
| Integration | Import glTF/texture/audio, scene load/save, physics step, audio mix buffer | Catch2 + fixtures |
| Render | Offscreen frames, golden images with tolerance, validation-layer clean | lavapipe + image compare |
| Headless smoke | Runtime boots, loads a scene, renders N frames, exits 0 | SDL dummy video + lavapipe |
| Performance | Frame-time budgets, import time, memory | Tracy + benchmark scripts |
| Fuzz | glTF/image/scene parsers (later) | libFuzzer |
| Manual | 2D/3D samples, editor checklist, GPU vendor matrix | Humans |

- CI: Ubuntu 24.04, GCC and Clang, debug + release, ASan/UBSan on tests, clang-format check, license check, shader compile, headless render test, and an offscreen swapchain/resize test on lavapipe.
- Golden tests disable TAA, jitter, auto-exposure, and time-based effects; use fixed seeds and fixed camera.
- lavapipe catches API misuse, not driver bugs or performance. Test on real NVIDIA/AMD/Intel hardware regularly.

### 14.4 Coding standards

- C++20; no modules in v1. Headers with `#pragma once`; include-what-you-use discipline.
- Namespaces `crab`, `crab::rhi`, `crab::scene`, etc.
- Types `PascalCase`; functions/methods `camelCase`; members `m_camelCase`; constants `kPascalCase`; files `snake_case`.
- `snake_case` for JSON keys and Lua bindings; be consistent.
- Prefer values and `std::span`/`std::string_view`; avoid owning raw pointers; `unique_ptr` for ownership, handles for references.
- Comments explain **why**, not **what**; public headers get API docs.
- Every irreversible decision gets an ADR in `docs/adr/`.

---

## 15. Phased Roadmap

The order below is the **generic reference**. The **selected 3D-first order** for the first game is in [GAME_3D_THIRD_PERSON.md](GAME_3D_THIRD_PERSON.md): it moves 3D PBR to Phase 5, extends Phase 6 with navigation, moves UI before the slice, and defers the 2D renderer to post-v1. The table remains useful if you later start a 2D-first project.

| Phase | Focus | Estimate (solo, full-time) | Gate |
|---:|---|---:|---|
| 0 | Bootstrap, build, CI, logging, window | 1–2 weeks | Bootstrap + window |
| 1 | Platform, input, jobs, time, events | 2–3 weeks | Fixed-step loop + profiler |
| 2 | Vulkan RHI bring-up + ImGui | 3–4 weeks | Triangle + ImGui, no validation errors |
| 3 | Render graph, minimal 3D/2D, camera | 4–5 weeks | Textured 3D cube + sprite, resize-safe |
| 4 | Scene/ECS, assets, serialization | 4–6 weeks | glTF scene load/save round-trip |
| 5 | 2D renderer + text | 3–4 weeks | Playable 2D demo (C++) |
| 6 | Physics 2D/3D (+ navigation on the 3D-first track) | 4–5 weeks (6–8 with navigation) | Character controller + collisions (+ navmesh) |
| 7 | Animation 2D/3D | 4–5 weeks | State machine + events |
| 8 | Audio | 2–3 weeks | Music/SFX/3D audio + buses |
| 9 | Scripting + gameplay framework | 3–4 weeks | 2D vertical slice in Lua |
| 10 | 3D PBR renderer + post | 5–6 weeks | Lit PBR scene at 1080p/60 |
| 11 | Runtime UI (RmlUi) | 3–4 weeks | Menu + HUD + gamepad nav |
| 12 | Tools/polish: particles, tilemap, save/load, localization | 4–6 weeks | Content-complete sample |
| 13 | Editor MVP | 6–8 weeks | Create/save/play a scene |
| 14 | Vertical slice, hardening, packaging | 4–6 weeks | Packaged game on clean Ubuntu |

**Sum:** ~52–72 weeks optimistic (add 2–3 weeks for navigation). Apply a 1.5× multiplier for reality: **18–24 months full-time** to a solid v1.0. The selected 3D-first track is ~53–74 weeks optimistic.

### Milestones

> The table below follows the generic phase order. The **selected 3D-first milestones** (including the vertical slice) are in [GAME_3D_THIRD_PERSON.md](GAME_3D_THIRD_PERSON.md#6-milestones).

| Milestone | Target | Proves |
|---|---|---|
| M0 | Week 2 | Toolchain, CI, window, logging |
| M1 | Week 6 | Vulkan device, swapchain, shaders, ImGui |
| M2 | Week 11 | Render graph, 3D mesh, 2D sprite, camera |
| M3 | Week 17 | Data-driven scene with imported glTF assets |
| M4 | Week 21 | Playable 2D demo |
| M5 | Week 27 | Physics + animation integrated |
| M6 | Week 33 | Audio + Lua + 2D vertical slice |
| M7 | Week 39 | 3D PBR showcase |
| M8 | Week 46 | UI + content tools |
| M9 | Week 54 | Editor MVP |
| M10 | Week 60+ | Packaged game, v1.0 |

These dates assume no major rewrites and no art production. Track them, but treat them as smoke detectors, not promises.

> **Ordering note:** Phases 5–10 below follow the generic table: Phase 5 is the 2D renderer and Phase 10 is 3D PBR. In the **selected 3D-first track**, run the Phase 10 3D PBR content at position 5, extend Phase 6 with navigation, move UI before the slice, and move Phase 5's 2D content to post-v1. The phase text itself remains the implementation checklist.

### Phase 0 — Bootstrap (1–2 weeks)

> **Implementation note (2026-10-19):** Complete. The repository now has modular
> `crab_core`, `crab_platform`, and `crab_engine` libraries, a runtime CLI,
> CMake presets (debug/release/ASan/Clang/headless), pinned SDL3/GLM/spdlog/
> Catch2 dependencies, structured logging and assertions, Catch2 tests, a
> headless smoke test, clang-format checks, and CI. Tracy is available behind
> `CRAB_ENABLE_TRACY`; the Vulkan dependency set (volk/VMA/vk-bootstrap/ImGui)
> moves to Phase 2 so the bootstrap stays small and fully testable.

**Tasks**
- Create repo, choose license (MIT or Apache-2.0), add `README`, `.gitignore`, `.clang-format`, `.clang-tidy`, `docs/`.
- CMake skeleton, presets, dependency aggregator, version header, `compile_commands.json`.
- FetchContent baseline: SDL3, GLM, spdlog, Catch2, Tracy, Dear ImGui, volk, VMA, vk-bootstrap.
- Logging, assertions, fatal handler, basic `main`, SDL3 window + event loop.
- GitHub Actions: build debug/release, tests, format check; ccache.
- First ADRs: scene model, Vulkan-only, editor-last, dependency policy.

**Exit criteria:** clean clone builds and opens a window on Ubuntu 24.04 with the bootstrap script and standard build commands; CI is green; `clang-format` passes.

### Phase 1 — Platform and Core Runtime (2–3 weeks)

**Tasks**
- Window abstraction (fullscreen, borderless, DPI, resize, minimize), Vulkan surface hook.
- Input: keyboard/mouse/gamepad, action map, deadzones, rebinding, text input/IME.
- Time: monotonic clock, fixed timestep, frame delta clamp, pause/time scale.
- Memory: tracking allocator, frame arena, handle system.
- Jobs: enkiTS wrapper, `parallel_for`, job counters, Tracy integration.
- Events: typed bus + deferred queue; config/JSON loading; filesystem/XDG paths.
- Proto-editor: ImGui debug overlay with logs, frame stats, input state.

**Exit criteria:** sample app with action-mapped input, fixed/variable updates, parallel job demo, and a Tracy capture; unit tests for time/events/handles.

### Phase 2 — Vulkan RHI Bring-up (3–4 weeks)

> **Implementation note (2026-10-20):** Implemented. `crab_rhi` exposes a
> pimpl-based `crab::rhi::Renderer` facade; Vulkan types stay out of public
> headers. The backend uses Vulkan 1.3 with dynamic rendering, synchronization2,
> volk, vk-bootstrap, VMA, and two frames in flight. It supports both a windowed
> SDL3 surface/swapchain path (FIFO, resize and minimize recovery) and a
> headless offscreen target with first-frame readback. GLSL shaders are compiled
> to SPIR-V by the `crab_shaders` target, a generated checker texture is sampled
> by a full-screen triangle, and the Dear ImGui SDL3/Vulkan backends render a
> debug overlay. Debug runs request `VK_LAYER_KHRONOS_validation`; validation
> errors are logged and counted in `RendererStats::validationErrors`. RenderDoc
> capture instructions are in [RENDERDOC.md](RENDERDOC.md). The full desktop
> runtime still needs to be manually exercised for ten minutes on a machine with
> X11/Wayland and a display; CI and local headless runs cover the device,
> shader, overlay, and offscreen readback paths.

**Tasks**
- Instance, debug messenger, physical/logical device selection, queues, surface.
- Swapchain: format, present mode, resize/minimize, out-of-date handling.
- VMA allocator, buffers, images, samplers, staging upload ring.
- Command buffers, frames in flight, fences/timeline semaphores, acquire/present.
- Shader compilation (`glslc`), SPIR-V reflection, pipeline layout, graphics pipeline.
- Descriptor sets, uniform buffers, push constants; ImGui Vulkan backend.
- Validation layers, debug labels, RenderDoc capture instructions.

**Exit criteria:** textured triangle (or full-screen quad) with no validation errors; resize/minimize stable for 10 minutes; ImGui overlay renders; first RenderDoc capture documented.

### Phase 3 — Render Graph + Minimal Renderers (4–5 weeks)

**Tasks**
- Render graph: pass declarations, transient resources, barriers, layout transitions, debug dump.
- Camera abstraction: perspective/ortho, view/projection, reverse-Z, viewport.
- Minimal 3D pass: depth buffer, unlit/textured mesh, sky clear, fly camera, debug lines/grid.
- Minimal 2D pass: orthographic camera, textured quad batch, sprite draw.
- Compute pipeline support; GPU timestamp queries; pipeline cache.
- Culling on jobs; flat `RenderWorld` extraction API.
- ImGui stats: CPU/GPU frame time, draw calls, triangles, memory.

**Exit criteria:** 3D cube/sphere and 2D sprites render into the same frame; render graph visualization works; resize and multi-pass ordering are correct; 60 fps on the dev machine.

### Phase 4 — Scene, Assets, Serialization (4–6 weeks)

**Tasks**
- EnTT world, entity IDs, hierarchy, transform propagation, lifecycle, deferred commands.
- Core components: identity, transform, hierarchy, camera, mesh, sprite, light, script placeholder.
- Reflection registration via `entt::meta`; inspector-ready property metadata.
- Asset DB: GUIDs, `.meta`, import cache, dependency graph, async loads, hot reload.
- Importers: glTF/GLB (meshes, materials, skins, animations, cameras, lights), textures, audio, fonts.
- Scene JSON serialization, `project.crab`, versioning, migrations, round-trip tests.
- Asset browser data model (no editor UI yet); placeholder assets.

**Exit criteria:** import a glTF scene into the world, render it, save/load the scene with identical data, hot-reload a texture/mesh; unit tests cover round-trips and GUID moves.

### Phase 5 — 2D Renderer and Text (3–4 weeks)

**Tasks**
- Canvas layers, sprite batching, atlas support, z/y-sort, blend modes.
- Sprite component, animation frames (basic), tint/flip, 9-slice.
- Tilemap runtime: chunked tiles, tileset asset, layers, Tiled import; collision data export.
- 2D camera: limits, smoothing, deadzone, pixel-perfect snapping.
- Text: FreeType rasterizer, glyph atlas, HarfBuzz shaping, line layout, font fallback.
- Debug text renderer; FPS/console overlay.
- First C++ 2D demo: parallax background, animated sprites, tilemap, camera follow.

**Exit criteria:** 2D demo runs at 4K/60; pixel-perfect at integer scales; text renders Latin + CJK fallback; atlas/sprite batch draw calls stay under 200.

### Phase 6 — Physics (4–5 weeks; 6–8 with navigation)

**Tasks**
- Jolt: system, layers, shapes, bodies, motion quality, CCD, sleeping, events.
- Physics ↔ ECS sync, interpolation, teleport, fixed step integration.
- Character controller (slope, step, ground snap, moving platforms).
- Queries: ray/shape casts, overlaps; collision layers/masks in project settings.
- Box2D: world, bodies, shapes, joints, sensors, queries, sync.
- **Navigation (3D-first track):** Recast/Detour bake, navmesh asset, agent component, path following, debug draw.
- Debug draw for colliders, contacts, broadphase; editor toggle data.
- 2D platformer controller on Box2D; 3D character demo on Jolt.

**Exit criteria:** 3D capsule walks on slopes/steps and pushes boxes; 2D platformer collides/jumps; events fire once per contact; no body leaks over 10 minutes. With navigation: an enemy agent patrols and chases the player around a gray-box level.

### Phase 7 — Animation (4–5 weeks)

**Tasks**
- ozz integration: skeleton, clips, sampling, blending, layers.
- glTF skin/animation import; bone palette upload; GPU skinning.
- State machine asset: states, parameters, transitions, blend spaces, sync groups.
- Events/markers; root motion; two-bone IK/foot placement.
- 2D sprite animation: frames, FPS, loop/ping-pong, frame events.
- Animation preview data for the future editor.

**Exit criteria:** character blends idle/walk/run by speed; animation events dispatch to gameplay callbacks (audio hookup lands in Phase 8); root motion moves the character controller correctly; 2D sprite animation loops with events.

### Phase 8 — Audio (2–3 weeks)

**Tasks**
- miniaudio device, format conversion, hotplug, default-device changes.
- Mixer graph, buses, sends, master limiter, voice pool/priorities.
- Decoders (WAV/MP3/FLAC/OGG), streaming music, decoded one-shots.
- Spatial audio: listener, attenuation, panning, Doppler, distance filtering.
- Bus layout asset, snapshots/ducking, volume/mute/solo.
- Audio asset import settings; silent placeholder on failure.
- Stress test: 64 voices + music + UI, no dropouts.

**Exit criteria:** music, SFX, UI, and 3D positional sources mix correctly; bus effects and snapshots work; device switch during playback does not crash; CPU audio <1 ms.

### Phase 9 — Scripting and Gameplay Framework (3–4 weeks)

**Tasks**
- Lua 5.4 + sol2 VM, sandbox, module loading, error handling with stack traces.
- Bindings: entity, transform, components, input, time, physics queries, audio, events, assets, UI hooks, math (vec/quat/color).
- Lifecycle callbacks, coroutines (`wait`, `tween`), timers.
- Per-script hot reload; generated EmmyLua stubs; console/log bridge.
- C++ game module: stable C ABI, build via CMake, load/unload/reload, state in engine.
- Gameplay helpers: spawn/destroy, scene transitions, save/load, tags/layers, prefab instantiation.
- First 3D gameplay prototype: player and enemy logic in Lua on the gray-box level.

**Exit criteria:** the 3D prototype is built without engine recompilation; player/enemy logic is in Lua; script reload <2 s; C++ module reload preserves the world; a full loop (spawn → fight → death → respawn) works. A provisional ImGui/debug HUD is acceptable until Phase 11 (generic) / Phase 10 (selected track).

### Phase 10 — 3D PBR Renderer (5–6 weeks)

**Tasks**
- PBR material graph (metallic-roughness), texture slots, material instances.
- Clustered Forward+ light grid (compute), directional/point/spot lights.
- CSM shadows (4 cascades, stable snapping, PCF), shadow atlas.
- IBL: irradiance SH, prefiltered specular, BRDF LUT; sky model; ambient.
- Exposure, bloom, tonemap (ACES/AgX), color grading LUT, FXAA.
- Transparency sorting, alpha modes, double-sided, instancing, GPU skinning.
- Fog (exponential/height), debug views (normals, depth, rough/metal, shadow cascades).
- 3D showcase: imported glTF environment, materials, lights, animated character.

**Exit criteria:** showcase renders at 1080p/60 on the target GPU; PBR reference spheres match expectations; no validation errors; GPU timings visible; quality toggles work.

### Phase 11 — Runtime UI (3–4 weeks)

**Tasks**
- RmlUi integration: render interface, input mapping, system interface, asset loading.
- Screen-space documents, data binding, templates, CSS theming, animations.
- Focus/keyboard/gamepad navigation, DPI scaling, safe areas.
- Pause menu, main menu, HUD, settings screen, loading screen.
- Text integration: use RmlUi's font engine initially; document the HarfBuzz upgrade path.
- UI ↔ gameplay events; localization-ready strings.

**Exit criteria:** complete menu/HUD flow with mouse, keyboard, and gamepad; 1080p/4K layouts; no text clipping at 125%/150% DPI; UI cost <1 ms.

### Phase 12 — Tools and Polish (4–6 weeks)

**Tasks**
- CPU particles (billboards/quads), emitter shapes, curves, sorting; GPU particles later.
- Tilemap painting data model and collision authoring (editor UI in Phase 13).
- Save/load slots, versioned save files, screenshots, settings persistence.
- Localization: string tables, font fallback, RTL hooks.
- Profiler overlay: frame graph, GPU timings, memory, asset loads, job utilization.
- LODs, mesh quantization, occlusion culling (optional), render scale.
- Documentation: getting started, API, samples, troubleshooting.
- Content-complete sample scene for both 2D and 3D.

**Exit criteria:** sample game has menus, save/load, particles, localization, and a stable 60 fps; docs let a new developer build and run in under 30 minutes.

### Phase 13 — Editor MVP (6–8 weeks)

**Tasks**
- `crab_editor` executable, ImGui docking layout, preferences, recent projects.
- Viewport: render target, editor camera, grid, selection, gizmos, play/pause/step/stop.
- Hierarchy: tree, reparenting, search, active/visible toggles.
- Inspector: reflection-driven properties, asset pickers, component add/remove, multi-edit.
- Asset browser: folders, thumbnails, import settings, drag/drop, rename/move, reimport.
- Console, stats, project settings (input, physics layers, audio buses, render quality).
- Undo/redo command stack; selection sync; scene save/load; play-mode process launch.
- Build integration: compile game module, show errors, run.
- Prefab create/instantiate (basic); tilemap paint mode; animation preview (stretch).

**Exit criteria:** create a new project, import assets, build a scene, assign scripts, save, play, and stop without corruption; undo/redo covers transforms and inspector edits; editor runs at 60 fps with 10k entities.

### Phase 14 — Vertical Slice and Hardening (4–6 weeks)

**Tasks**
- Build one complete small game with the engine: 10–30 minutes of play, menus, save/load, audio, animation, UI.
- Fix only what the game needs; no new engine features except critical gaps.
- Performance pass against budgets; memory leaks; long-session stability.
- Packaging: `crabpak`, runtime + game module + assets, `$ORIGIN` RPATH, desktop file, launcher.
- Clean-machine test on Ubuntu 24.04 without dev packages.
- Crash handling, logging, version display, license/attribution screen.
- Final docs, sample project, release checklist.

**Exit criteria:** packaged game runs on a clean Ubuntu 24.04 machine; 2-hour playtest without crash; CPU/GPU budgets met; no validation errors; all tests pass; `v1.0` tag.

### Post-v1 backlog (do not start before v1)

Terrain, water, volumetric fog, decals, SSR, GPU-driven culling, occlusion culling, ray tracing, SSAO/SSGI, HDR output, FSR/DLSS, 2D lights/shadows, Spine/DragonBones, nested prefabs, timeline/cutscene editor, visual scripting, C#/AngelScript, networking/rollback, Steam integration, modding SDK, Windows/macOS ports, mobile, console, VR.

---

## 16. Sample Games and Vertical Slices

The engine must be driven by games, not feature demos. The selected first game is the 3D third-person slice; the 2D slice is post-v1.

1. **3D gameplay prototype (after Phase 7):** gray-box level, capsule character, third-person camera, one enemy with navigation, basic combat. This is the go/no-go gate for feel.
2. **3D vertical slice (after Phase 11):** one environment, one enemy type, one objective, menus, HUD, audio, checkpoint, win/lose loop. This is the portfolio piece and the engine's proof.
3. **Release slice (Phase 14):** the same slice polished, optimized, and packaged end-to-end. It is the v1.0 proof.

**Post-v1 2D slice:** a small platformer/top-down game that exercises sprites, tilemaps, Box2D, 2D animation, and pixel-perfect cameras. It validates the deferred 2D renderer; do not start it before the 3D game ships.

Rules:
- The sample game lives in `game/` and uses only public engine APIs.
- No sample-specific hacks in the engine; if a hack is needed, fix the API.
- Every slice has a performance target and a 30-minute playtest checklist.
- Record a short GIF/video for each milestone; it exposes regressions and motivates progress.

---

## 17. Risk Register

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Scope creep / feature list grows | High | High | First game as customer; explicit v1 cut list; feature freeze before slices |
| Editor underestimated | High | High | MVP only; ImGui; build after runtime; separate process play mode |
| Asset pipeline becomes a project | High | Medium | Start with glTF + loose files + `.meta`; cook later; no streaming in v1 |
| Text/Unicode underestimated | Medium | High | FreeType/HarfBuzz; defer bidi/emoji; test CJK early |
| Animation integration complexity | Medium | High | ozz runtime; JSON state machine; defer morph targets/full-body IK |
| Third-person camera feel | High | High | Prototype camera before combat; sphere-cast collision; tune with mouse/gamepad |
| Navigation integration | Medium | High | Recast timebox; waypoint-steering fallback; low enemy count |
| 3D level/content production | High | Medium | Gray-box first; one modular kit; reuse; licensed assets |
| Audio effects/mixer time sink | Medium | Medium | miniaudio + timebox; SoLoud fallback; stable engine API |
| Vulkan driver differences | Medium | High | Validation layers; real GPU matrix; avoid obscure extensions |
| Wayland/IME/DPI issues | Medium | Medium | Test early on GNOME/KDE; SDL3; fallback X11 |
| Hot reload ABI breakage | Medium | High | C ABI from day one; state in engine; no STL across boundary |
| Performance regressions | Medium | High | Budgets, Tracy, CI goldens, profiling each phase |
| Over-engineering (bindless/GPU-driven/ECS purity) | High | Medium | YAGNI; implement tiers; measure before abstracting |
| Solo burnout / bus factor | High | High | Milestones, playable builds, docs/ADRs, breaks |
| License contamination | Low | High | Permissive-only policy; CI license check; notices |
| Testing gaps on real GPUs | Medium | Medium | Monthly manual matrix; community testers later |
| Physics/network determinism promises | Medium | Medium | Do not promise cross-platform determinism in v1 |

---

## 18. Performance Budgets

Target: 1080p/60 Hz, 16.6 ms frame, mid-range GPU (GTX 1660 / RX 580 class).

| Budget | Target | Notes |
|---|---:|---|
| Total CPU frame | < 8 ms | Main + jobs; leave headroom |
| Main-thread CPU | < 4 ms | Scene, scripts, submission |
| Render CPU | < 5 ms | Culling, graph, command recording |
| GPU frame | < 14 ms | Include post |
| Audio callback | < 1 ms | No allocation/locks |
| UI | < 1 ms | RmlUi + text |
| Draw calls (3D) | < 1,500 | Batch/instance aggressively |
| Triangles (3D) | < 2 M | Sample-dependent |
| Lights | < 256 | Clustered |
| Shadow cascades | 4 | 1024–2048 each |
| Texture memory | < 1.5 GB | Compressed where possible |
| 2D draw calls | < 200 | 10k sprites via batching |
| 2D target | 4K/60 | Mid-range GPU |
| Editor | 60 fps @ 10k entities | Viewport may be lower |
| Scene load (sample) | < 5 s | SSD, cooked assets |
| Hot reload | < 2 s | Shader/Lua/asset |
| Clean build | < 15 min | With ccache warm |

Measure with Tracy CPU zones, Vulkan timestamp queries, `VK_EXT_memory_budget`, and a frame-time graph. Budgets are merge gates for the sample games, not aspirational stickers.

---

## 19. Plan Review: A Senior Critique

This section is the part I would want a lead engineer to write before committing a year of work.

### 19.1 What is strong

- **Vulkan-first with a thin RHI and render graph** is the right long-term foundation. It avoids a GL/Vulkan abstraction tax and supports modern features without hacks.
- **Buying commodity systems** (Jolt, Box2D, ozz, FreeType/HarfBuzz, miniaudio, RmlUi) is correct. These are years of work each.
- **GUID + `.meta` + import cache** is the proven Godot/Unity approach and prevents years of asset-path pain.
- **ECS + hierarchy** gives performance and clean serialization while preserving a Godot-like tree in the editor.
- **Hot reload from day one** (shaders, assets, Lua, C++ module) is the highest-leverage productivity feature.
- **Editor-last** is the right sequencing, even though it feels wrong. The editor is a client of the runtime.
- **Headless CI with lavapipe + golden images** catches API misuse early and cheaply.

### 19.2 What I would change

1. **Cut the v1 feature list by 30–40%.** Terrain, water, weather, volumetric fog, SSR, GPU-driven rendering, nested prefabs, timeline, 2D skeletal animation, the 2D renderer, and networking are not v1. Every one is a mini-project.
2. **Use the selected 3D third-person game as the customer.** A responsive character, camera, enemy, and one level shape the engine. Without a customer, the feature list wins and the schedule loses.
3. **Do not build the editor before Phase 13.** Build an ImGui debug overlay early, but no asset browser/inspector/undo until the runtime can ship a game.
4. **Do not build a custom UI toolkit.** RmlUi is the pragmatic choice; ImGui is for tools only. A custom retained UI is 6–12 months.
5. **Do not build bindless/GPU-driven/indirect-first.** Start with per-frame descriptor sets and CPU culling; add bindless and indirect draws when the profiler demands it.
6. **Do not unify 2D and 3D passes.** Share the RHI, graph, materials, and text; keep separate renderers. Godot does the same.
7. **Design reflection, serialization, and GUIDs before gameplay.** Retrofitting them is brutal. Phase 4 is not optional infrastructure; it is the spine.
8. **Enforce the C ABI boundary for game modules from day one.** It is much harder to add later.
9. **Treat text as a first-class subsystem.** It touches UI, world labels, localization, and the editor. Budget 3–4 weeks, not 3 days.
10. **Use a fixed-step + interpolation model from the start.** Retrofitting it into physics/animation/camera is painful.
11. **Do not promise determinism or netcode.** They constrain every system and are post-v1.
12. **Add a 1.5× schedule multiplier.** The phase table is the best case with no rewrites, no art, and no life.
13. **Define "done" per phase and refuse to move on without the gate.** Otherwise Phase 2 leaks into Phase 6.
14. **Keep a decision log (ADRs).** Future-you will not remember why the RHI owns the swapchain or why Lua is main-thread-only.
15. **Measure iteration time, not just frame time.** A 60 fps engine with a 30-second import is still painful.

### 19.3 The uncomfortable question

If your primary goal is to **make games**, a custom engine is almost always the wrong economic choice. Godot 4 is free, MIT-licensed, and has a 2D/3D renderer, editor, animation, audio, physics, UI, and localization. GDExtension lets you write C++ modules for performance. You could ship a game this year.

Build Crab if:
- Engine/tooling is the product or a long-term strategic asset.
- You need control that Godot cannot give you (renderer, memory, platform, pipeline).
- You want to learn graphics/engine architecture deeply and have 1–2 years.
- You are building multiple games with a specific shared tech need.

Do not build Crab if:
- You want to validate a game idea quickly.
- You expect Godot parity in under a year.
- You are alone, part-time, and want a finished game soon.

There is no shame in either path. The worst outcome is 18 months of engine work with no shipped game.

### 19.4 Alternative tracks

| Track | Scope | Time to first game | When to choose |
|---|---|---|---|
| **A. Full engine** (this plan) | Runtime + editor + tools | 14–24 months | Engine is the product/long-term asset |
| **B. Framework-first** | Phases 0–9, no editor; JSON scenes + ImGui debug; external Blender/Tiled | 6–9 months | You want a game first, editor later |
| **C. Godot + GDExtension** | Godot + C++ modules | Weeks–months | You want to ship games now |

**Recommendation:** if you have not shipped a game before, do Track C once, then decide. If you have shipped games and know exactly what you need, do Track B, then grow it into Track A. Only commit to Track A with eyes open.

### 19.5 Kill criteria and pivot triggers

Set these **before** starting, while you are still objective:

| Trigger | Pivot |
|---|---|
| First playable 3D prototype slips more than 4 weeks past the milestone | Cut content, lock-on, second enemy, and VFX — not camera/character feel |
| Editor MVP exceeds 10 weeks | Ship the game with JSON scenes + ImGui debug tools; defer the editor |
| Custom audio mixer/effects exceed 3 weeks | Swap to SoLoud behind the existing audio API |
| Text shaping exceeds 4 weeks for the target languages | Ship Latin-only UI; defer bidi/emoji/CJK |
| 3D PBR showcase cannot hit 1080p/60 after two optimization passes | Reduce the target (720p/60 or 30 fps), not the whole renderer |
| Hot reload or import time exceeds 5 seconds | Stop feature work and fix iteration speed |
| You have not played the sample game for fun in a month | The engine is drifting from the game; re-anchor on the slice |

These are not failures. They are the plan working as intended: protecting the shipped game from the engine's ambition.

---

## 20. Definition of Done for v1.0

- [ ] Clean Ubuntu 24.04 build with the documented bootstrap script and build commands; CI green (GCC + Clang, debug + release).
- [ ] 2D and 3D samples run at their targets; no validation errors.
- [ ] Editor creates/opens/saves/plays a project; undo/redo works for transforms and inspector edits.
- [ ] Import pipeline handles glTF, textures, audio, fonts, shaders, Tiled maps; missing assets are visible, not fatal.
- [ ] Scene format is versioned and round-trip tested; GUID references survive moves.
- [ ] Physics (Jolt + Box2D), animation (ozz + sprite), audio buses/spatialization, and UI are integrated and documented.
- [ ] Lua scripting and C++ game module hot reload work; stubs generated.
- [ ] Packaged build runs on a clean machine without the Vulkan SDK.
- [ ] Crash handler, logs, version info, license screen; 2-hour playtest clean.
- [ ] Performance budgets met; Tracy captures available.
- [ ] Documentation: getting started, architecture, API, samples, troubleshooting, contribution/style.
- [ ] `THIRD_PARTY_NOTICES.md` complete; engine license chosen.

---

## 21. First 10 Working Days (Concrete)

| Day | Task | Done when |
|---:|---|---|
| 1 | Install prerequisites; create repo, license, `.gitignore`, `README` | `git status` clean; tools installed |
| 2 | CMake skeleton + presets + dependency aggregator; CI workflow | `cmake --preset linux-debug` configures |
| 3 | Logging, assertions, version header, `main`; SDL3 window | Window opens/closes with logs |
| 4 | Input state + action map skeleton; frame loop with fixed time | Print actions; fixed tick counter |
| 5 | volk/VMA/vk-bootstrap; instance/device/queues; validation | Device name logged; no validation errors |
| 6 | Swapchain, frames in flight, clear screen, resize | Stable clear color for 10 minutes |
| 7 | `glslc` shader pipeline; triangle/full-screen quad | Shader hot reload works |
| 8 | ImGui backend + debug overlay; Tracy | Frame stats overlay; Tracy capture |
| 9 | Unit test target; first tests (time, handles, events); format check | `ctest` green; CI green |
| 10 | ADRs + architecture doc; demo GIF; backlog for Phase 1 | Team can explain the plan |

**First commands**

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
./build/linux-debug/crab_runtime
```

If these do not work on a clean machine, stop and fix the developer experience before writing engine features. Build friction compounds.

---

## Appendix A. CMake and Presets Sketch

```cmake
cmake_minimum_required(VERSION 3.28)
project(Crab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CRAB_BUILD_EDITOR "Build the editor" ON)
option(CRAB_BUILD_TESTS  "Build tests" ON)
option(CRAB_ENABLE_VALIDATION "Enable Vulkan validation" ON)

add_subdirectory(third_party)
add_subdirectory(engine)
if (CRAB_BUILD_EDITOR)
  add_subdirectory(editor)
endif()
add_subdirectory(runtime)
add_subdirectory(game)
if (CRAB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Shaders: one custom command per GLSL file, output to ${CMAKE_BINARY_DIR}/shaders
```

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "linux-debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/linux-debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_COMPILER": "clang++",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "CRAB_ENABLE_VALIDATION": "ON"
      }
    },
    {
      "name": "linux-release",
      "inherits": "linux-debug",
      "binaryDir": "${sourceDir}/build/linux-release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "RelWithDebInfo" }
    }
  ],
  "buildPresets": [
    { "name": "linux-debug", "configurePreset": "linux-debug" },
    { "name": "linux-release", "configurePreset": "linux-release" }
  ],
  "testPresets": [
    {
      "name": "linux-debug",
      "configurePreset": "linux-debug",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

---

## Appendix B. API Sketches

**Engine boot**

```cpp
#include <crab/crab.hpp>

int main(int argc, char** argv) {
  crab::EngineConfig cfg;
  cfg.project_path = "project.crab";
  cfg.headless = false;

  crab::Engine engine(cfg);
  if (!engine.init()) return 1;
  engine.run();
  engine.shutdown();
  return 0;
}
```

**Creating a scene object**

```cpp
crab::World& world = engine.world();
crab::Entity player = world.create("Player");

auto& t = world.emplace<crab::Transform>(player);
t.position = {0.0f, 0.0f, 0.0f};

world.emplace<crab::MeshRenderer>(player, mesh_handle, material_handle);
world.emplace<crab::RigidBody>(player, crab::BodyType::Dynamic);
world.emplace<crab::Collider>(player, crab::CapsuleShape{0.4f, 1.8f});
world.emplace<crab::Script>(player, lua_asset_handle);
```

**Render extraction**

```cpp
crab::RenderWorld rw;
crab::extract_render_world(world, rw, active_camera);
renderer.render(rw);
```

**C++ game module ABI**

```c
/* Public C header: crab_game_api.h */
typedef struct crab_game_context crab_game_context; /* opaque */

CRAB_GAME_API void crab_game_init(crab_game_context* ctx);
CRAB_GAME_API void crab_game_update(crab_game_context* ctx, float dt);
CRAB_GAME_API void crab_game_fixed_update(crab_game_context* ctx, float dt);
CRAB_GAME_API void crab_game_shutdown(crab_game_context* ctx);
```

`crab_game_context` is an opaque handle table owned by the runtime; only C-compatible types and function pointers cross the boundary. The runtime owns the world, so reloading the module does not destroy game state.

---

## Appendix C. Glossary

| Term | Meaning |
|---|---|
| **RHI** | Render Hardware Interface; Crab's thin Vulkan abstraction |
| **Render graph** | Declarative frame description that derives barriers and transient resources |
| **Bindless** | Descriptor indexing: textures/samplers indexed from a large array in shader |
| **Forward+** | Forward renderer with clustered light culling |
| **CSM** | Cascaded shadow maps for directional lights |
| **IBL** | Image-based lighting from a prefiltered environment map |
| **PBR** | Physically based rendering (metallic-roughness) |
| **ECS** | Entity-component-system; data-oriented storage and queries |
| **GUID** | Globally unique asset/entity identifier; stable across renames |
| **Hot reload** | Replacing code/assets at runtime without restarting |
| **Reverse-Z** | Depth mapping with near=1, far=0 for better precision |
| **Lavapipe** | Mesa's software Vulkan implementation, used in CI |
| **ADR** | Architecture decision record; documents irreversible choices |
| **Vertical slice** | A small but complete game path proving all systems together |

---

## Appendix D. Final Recommendation

Build in this order:

1. **Window → RHI → render graph → one textured 3D mesh.**
2. **Scene + assets + glTF import + save/load.**
3. **3D PBR + lighting + post.**
4. **Character controller + navigation + animation + audio + Lua.**
5. **UI + tools + editor.**
6. **Ship the 3D third-person slice; then generalize.**

Do not start with the editor, terrain, networking, or a custom scripting language. Do not add a feature that the current sample game does not use. Keep the engine boring, measured, and playable.

The plan is ambitious but coherent. The difference between success and a two-year unfinished engine is **scope discipline**, not Vulkan knowledge.
