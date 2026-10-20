# Capturing a Frame with RenderDoc

Crab's Phase 2 renderer is a single full-screen triangle plus the Dear ImGui
overlay, which is enough to validate the instance/device/swapchain setup,
dynamic rendering, descriptor binding, and present path before the render graph
lands in Phase 3.

## Prerequisites

- A desktop build (`linux-debug`, `linux-release`, or `linux-clang`).
- A working Vulkan 1.3 driver.
- [RenderDoc](https://renderdoc.org/) 1.30 or newer.
- Optional: the Khronos validation layer (`vulkan-validationlayers` on Ubuntu).

## Capture from the RenderDoc UI

1. Launch RenderDoc and choose **File → Launch Application**.
2. Set **Executable Path** to:
   `build/linux-debug/bin/crab`
3. Set **Working Directory** to the repository root.
4. Leave the command-line arguments empty for the windowed path, or use
   `--frames 5` for a short capture that exits automatically.
5. Press **Launch**. Once the window is visible, press `F12` (RenderDoc's
   default capture key) or use the frame-capture button.
6. Close the window normally so the swapchain and all Vulkan objects are
   destroyed cleanly, then inspect the capture in RenderDoc.

The debug overlay can be toggled with `F1`; capture once with it visible and
once with it hidden to compare the triangle-only and ImGui draw lists.

## Capture from the command line

```bash
renderdoccmd capture \
  --working-dir "$PWD" \
  --capture-file /tmp/crab-capture.rdc \
  ./build/linux-debug/bin/crab --frames 3
```

Open `/tmp/crab-capture.rdc` in the RenderDoc UI. `--frames 3` makes the
runtime terminate after three frames, which is convenient in scripts.

## What to check

- **Pipeline State:** `fullscreen.vert` / `fullscreen.frag`, dynamic rendering,
  `VK_FORMAT_B8G8R8A8_SRGB` (or the chosen fallback), and the descriptor set
  with the checker texture plus uniform buffer.
- **Texture Viewer:** the checkerboard pattern modulated by the animated pulse
  and vignette; the ImGui window appears in the same render pass.
- **Mesh Viewer:** one full-screen triangle from `vkCmdDraw(3, 1, 0, 0)`.
- **API Inspector:** image layout transitions from `UNDEFINED`/`PRESENT_SRC` to
  `COLOR_ATTACHMENT_OPTIMAL` and back to `PRESENT_SRC`.
- **Performance:** the overlay reports the frame time, FPS, draw calls, and
  triangle count.

## Validation layers

Debug builds request `VK_LAYER_KHRONOS_validation` when it is available. If the
layer is installed outside the loader's default search path, point the loader at
it:

```bash
VK_LAYER_PATH=/path/to/explicit_layer.d \
LD_LIBRARY_PATH=/path/to/layer/libs \
./build/linux-debug/bin/crab
```

The runtime logs validation messages under the `vulkan` category and counts
errors in the overlay. Headless and unit-test runs also expose the count as
`RendererStats::validationErrors`; the test suite treats a non-zero count as a
failure.

## Headless rendering

The offscreen path has no swapchain or presentation, but it exercises the same
shader, descriptors, dynamic-rendering pipeline, and image barriers. Use it for
CI and golden-image work:

```bash
./build/linux-headless/bin/crab --headless --frames 3
```

The first headless frame is read back and logged as corner/center RGBA values.
