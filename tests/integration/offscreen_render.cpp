#include "crab/core/log.hpp"
#include "crab/platform/platform.hpp"
#include "crab/platform/window.hpp"
#include "crab/rhi/renderer.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>

namespace {

int fail(const char* message, const crab::rhi::Renderer& renderer) {
  SDL_Log("offscreen_render: %s (%s)", message, renderer.lastError().c_str());
  return EXIT_FAILURE;
}

bool renderFrames(crab::rhi::Renderer& renderer, int count) {
  for (int frame = 0; frame < count; ++frame) {
    if (!renderer.beginFrame()) {
      return false;
    }
    crab::rhi::OverlayStats overlay;
    overlay.frameTimeMs = 16.0;
    overlay.framesPerSecond = 60.0;
    overlay.frameIndex = static_cast<std::uint64_t>(frame);
    renderer.drawDebugOverlay(overlay);
    if (!renderer.endFrame()) {
      return false;
    }
  }
  return true;
}

} // namespace

int main() {
  crab::core::LogConfig logConfig;
  logConfig.console = true;
  logConfig.file = false;
  crab::core::initializeLogging(logConfig);

  if (!crab::platform::initialize()) {
    return EXIT_FAILURE;
  }

  crab::platform::WindowConfig windowConfig;
  windowConfig.title = "Crab offscreen render test";
  windowConfig.width = 320;
  windowConfig.height = 240;
  auto window = crab::platform::Window::create(windowConfig);
  if (window == nullptr) {
    crab::platform::shutdown();
    return EXIT_FAILURE;
  }

  crab::rhi::Renderer renderer;
  if (!renderer.initialize(*window)) {
    const int result = fail("renderer initialization failed", renderer);
    window.reset();
    crab::platform::shutdown();
    crab::core::shutdownLogging();
    return result;
  }

  if (!renderFrames(renderer, 2)) {
    const int result = fail("initial frame submission failed", renderer);
    renderer.shutdown();
    window.reset();
    crab::platform::shutdown();
    crab::core::shutdownLogging();
    return result;
  }

  // Resize the window and ask the renderer to recreate its swapchain. The
  // offscreen SDL video driver reports the new pixel size synchronously.
  if (!SDL_SetWindowSize(window->nativeHandle(), 400, 300)) {
    SDL_Log("offscreen_render: SDL_SetWindowSize failed: %s", SDL_GetError());
    renderer.shutdown();
    window.reset();
    crab::platform::shutdown();
    crab::core::shutdownLogging();
    return EXIT_FAILURE;
  }
  SDL_SyncWindow(window->nativeHandle());
  SDL_PumpEvents();
  renderer.requestResize(400, 300);

  if (!renderFrames(renderer, 3)) {
    const int result = fail("frame submission after resize failed", renderer);
    renderer.shutdown();
    window.reset();
    crab::platform::shutdown();
    crab::core::shutdownLogging();
    return result;
  }

  const crab::rhi::RendererStats stats = renderer.stats();
  if (stats.swapchainWidth != 400 || stats.swapchainHeight != 300) {
    SDL_Log("offscreen_render: unexpected swapchain extent %ux%u", stats.swapchainWidth,
            stats.swapchainHeight);
    renderer.shutdown();
    window.reset();
    crab::platform::shutdown();
    crab::core::shutdownLogging();
    return EXIT_FAILURE;
  }
  if (stats.validationErrors != 0) {
    SDL_Log("offscreen_render: %u validation errors", stats.validationErrors);
    renderer.shutdown();
    window.reset();
    crab::platform::shutdown();
    crab::core::shutdownLogging();
    return EXIT_FAILURE;
  }

  SDL_Log("offscreen_render: resize %ux%u -> %ux%u, %llu frames, validation errors: %u",
          windowConfig.width, windowConfig.height, stats.swapchainWidth, stats.swapchainHeight,
          static_cast<unsigned long long>(stats.frameIndex), stats.validationErrors);

  renderer.shutdown();
  window.reset();
  crab::platform::shutdown();
  crab::core::shutdownLogging();
  return EXIT_SUCCESS;
}
