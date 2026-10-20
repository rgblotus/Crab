#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace crab::platform {
class Window;
}

namespace crab::rhi {
class Renderer;
}

namespace crab {

struct ApplicationConfig {
  std::string title = "Crab";
  int width = 1280;
  int height = 720;
  bool headless = false;
  bool resizable = true;
  bool highPixelDensity = true;
  bool vulkan = true;
  double targetFrameRate = 60.0;
  std::uint64_t maxFrames = 0; // 0 = run until quit
};

class Application {
public:
  explicit Application(ApplicationConfig config = {});
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  bool init();
  int run();
  void shutdown();
  void requestExit() noexcept;

  [[nodiscard]] bool isRunning() const noexcept;
  [[nodiscard]] std::uint64_t frameCount() const noexcept;
  [[nodiscard]] double elapsedSeconds() const noexcept;
  [[nodiscard]] platform::Window* window() noexcept;
  [[nodiscard]] rhi::Renderer* renderer() noexcept;

private:
  void processEvents();

  ApplicationConfig m_config;
  std::unique_ptr<platform::Window> m_window;
  std::unique_ptr<rhi::Renderer> m_renderer;
  bool m_initialized = false;
  bool m_running = false;
  std::uint64_t m_frameCount = 0;
  double m_startTime = 0.0;
  double m_frameTimeMs = 0.0;
  double m_framesPerSecond = 0.0;
};

} // namespace crab
