#pragma once

#include <cstdint>
#include <memory>
#include <string>

union SDL_Event;

namespace crab::platform {
class Window;
}

namespace crab::rhi {

struct RendererConfig {
#ifdef NDEBUG
  bool validation = false;
#else
  bool validation = true;
#endif
  bool preferDiscreteGpu = true;
  bool enableImGui = true;
  std::string applicationName = "Crab";
  std::string engineName = "Crab";
};

struct RendererStats {
  std::uint64_t frameIndex = 0;
  std::string deviceName;
  std::uint32_t swapchainWidth = 0;
  std::uint32_t swapchainHeight = 0;
  std::uint32_t swapchainImageCount = 0;
  std::uint32_t framesInFlight = 0;
  std::uint32_t drawCalls = 0;
  std::uint32_t triangles = 0;
  std::uint32_t lines = 0;
  std::uint32_t renderPasses = 0;
  std::uint32_t validationErrors = 0;
  bool headless = false;
  bool imgui = false;
  bool validation = false;
};

struct OverlayStats {
  double frameTimeMs = 0.0;
  double framesPerSecond = 0.0;
  std::uint64_t frameIndex = 0;
  std::string deviceName;
};

// The Phase 2 RHI facade. It intentionally keeps Vulkan out of the public
// engine headers; later phases will grow explicit low-level resources behind
// this boundary.
class Renderer {
public:
  Renderer();
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;

  bool initialize(platform::Window& window, const RendererConfig& config = {});
  bool initializeHeadless(int width, int height, const RendererConfig& config = {});

  void shutdown();
  void waitIdle();

  [[nodiscard]] bool isInitialized() const noexcept;
  [[nodiscard]] bool isHeadless() const noexcept;
  [[nodiscard]] bool isFrameAborted() const noexcept;
  [[nodiscard]] const std::string& lastError() const noexcept;
  [[nodiscard]] RendererStats stats() const noexcept;

  // Returns false when there is no image to render into (for example while the
  // window is minimized). endFrame() must only be called after a true result.
  bool beginFrame();
  void drawDebugOverlay(const OverlayStats& stats);
  bool endFrame();

  void processEvent(const SDL_Event& event);
  void requestResize(int width, int height);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace crab::rhi
