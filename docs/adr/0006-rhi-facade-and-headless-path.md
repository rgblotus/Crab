# 0006 — Keep Vulkan behind an RHI facade and support a first-class headless path

- **Status:** Accepted
- **Date:** 2026-10-20

## Context

Phase 2 needs to prove the Vulkan 1.3 device, swapchain, shader pipeline, and
ImGui overlay without leaking Vulkan headers into gameplay or editor code. CI
and automated tests also need to exercise rendering on machines with no display,
where a swapchain is impossible.

## Decision

`crab::rhi::Renderer` is a pimpl facade with a deliberately small lifecycle:
initialize (windowed or headless), begin/draw/end frame, resize notification,
stats, and shutdown. Vulkan headers, VMA, vk-bootstrap, and volk are private to
`crab_rhi`.

The backend has two targets behind the same pipeline and descriptor code:

- **Windowed:** SDL3 `VkSurfaceKHR`, vk-bootstrap swapchain, FIFO present, two
  frames in flight, acquire/present semaphores, and out-of-date/minimize
  recovery by recreating the swapchain and pipeline when the format changes.
- **Headless:** an offscreen color image with the same dynamic-rendering
  pipeline and a host-visible readback buffer. The first frame is read back to
  verify that shader output reached memory.

Shaders are compiled offline with `glslc` into SPIR-V. The runtime resolves
them from the build directory, the executable directory, or the current
working directory, in that order.

## Consequences

- Gameplay and editor code can be written before the render graph exists and
  without including Vulkan headers.
- CI can run the real shader/pipeline path with Mesa lavapipe.
- The facade is intentionally narrow. Low-level GPU resources will be added in
  Phase 3 behind explicit RHI handles as the render graph requires them; we are
  not pretending this is a cross-API abstraction.
- The windowed swapchain/present path is covered after surface creation by an
  SDL offscreen-video integration test running on Mesa lavapipe, including a
  resize and zero validation errors. Real desktop window/compositor behavior
  still needs a manual ten-minute resize/minimize run plus a RenderDoc capture.
