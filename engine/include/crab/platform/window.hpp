#pragma once

#include <memory>
#include <string>
#include <string_view>

struct SDL_Window;

namespace crab::platform {

struct WindowConfig {
  std::string title = "Crab";
  int width = 1280;
  int height = 720;
  bool resizable = true;
  bool highPixelDensity = true;
  bool vulkan = true;
  bool hidden = false;
};

class Window {
public:
  static std::unique_ptr<Window> create(const WindowConfig& config = {});

  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

  void requestClose() noexcept;
  [[nodiscard]] bool shouldClose() const noexcept;

  void setTitle(std::string_view title);

  [[nodiscard]] int width() const noexcept;
  [[nodiscard]] int height() const noexcept;
  [[nodiscard]] int logicalWidth() const noexcept;
  [[nodiscard]] int logicalHeight() const noexcept;
  [[nodiscard]] float displayScale() const noexcept;

  [[nodiscard]] SDL_Window* nativeHandle() const noexcept;

private:
  explicit Window(SDL_Window* window) noexcept;

  SDL_Window* m_window = nullptr;
  bool m_shouldClose = false;
};

} // namespace crab::platform
