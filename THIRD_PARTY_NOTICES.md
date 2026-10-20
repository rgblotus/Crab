# Third-Party Notices

Crab is licensed under the MIT License (see `LICENSE`). The following third-party
projects are used by the engine. Versions are pinned in `third_party/CMakeLists.txt`;
consult the upstream projects for complete license texts.

| Project | Purpose | License |
|---|---|---|
| [SDL3](https://github.com/libsdl-org/SDL) | Window, input, platform layer | zlib |
| [spdlog](https://github.com/gabime/spdlog) | Logging | MIT |
| [fmt](https://github.com/fmtlib/fmt) | Formatting (bundled with spdlog) | MIT |
| [GLM](https://github.com/g-truc/glm) | Math | MIT |
| [Catch2](https://github.com/catchorg/Catch2) | Unit testing | BSL-1.0 |
| [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) | Vulkan API headers | Apache-2.0 |
| [volk](https://github.com/zeux/volk) | Vulkan entry-point loader | MIT |
| [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | GPU memory allocation | MIT |
| [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) | Instance/device/swapchain setup | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) | Debug overlay and future editor UI | MIT |
| [Tracy](https://github.com/wolfpld/tracy) | Optional profiler (`CRAB_ENABLE_TRACY`) | BSD-3-Clause |

Future phases add EnTT (MIT), Jolt (MIT), Box2D (MIT), ozz-animation (MIT),
cgltf (MIT), meshoptimizer (MIT), FreeType (FTL), HarfBuzz (MIT), RmlUi (MIT),
miniaudio (MIT-0/public domain), dr_libs (MIT-0/public domain), Lua (MIT),
sol2 (MIT), Recast & Detour (zlib), and others. This file will be updated as
dependencies are added.

No third-party code is copied into Crab without attribution. Update this file
whenever a dependency is added or upgraded.
