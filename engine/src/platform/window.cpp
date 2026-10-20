#include "crab/platform/window.hpp"

#include "crab/core/log.hpp"

#include <SDL3/SDL.h>

namespace crab::platform {
namespace {

SDL_WindowFlags makeFlags(const WindowConfig& config) noexcept {
  SDL_WindowFlags flags = 0;
  if (config.resizable) {
    flags |= SDL_WINDOW_RESIZABLE;
  }
  if (config.highPixelDensity) {
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
  }
  if (config.vulkan) {
    flags |= SDL_WINDOW_VULKAN;
  }
  if (config.hidden) {
    flags |= SDL_WINDOW_HIDDEN;
  }
  return flags;
}

} // namespace

std::unique_ptr<Window> Window::create(const WindowConfig& config) {
  const SDL_WindowFlags flags = makeFlags(config);
  SDL_Window* handle = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
  if (handle == nullptr) {
    CRAB_LOG_ERROR("window", "SDL_CreateWindow failed: {}", SDL_GetError());
    return nullptr;
  }

  auto window = std::unique_ptr<Window>(new Window(handle));
  CRAB_LOG_INFO("window", "Created '{}' ({}x{} logical, flags=0x{:x})", config.title, config.width,
                config.height, static_cast<unsigned int>(flags));
  return window;
}

Window::Window(SDL_Window* window) noexcept : m_window(window) {}

Window::~Window() {
  if (m_window != nullptr) {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }
}

void Window::requestClose() noexcept {
  m_shouldClose = true;
}

bool Window::shouldClose() const noexcept {
  return m_shouldClose;
}

void Window::setTitle(std::string_view title) {
  if (m_window != nullptr) {
    SDL_SetWindowTitle(m_window, std::string(title).c_str());
  }
}

int Window::width() const noexcept {
  int width = 0;
  int height = 0;
  if (m_window != nullptr) {
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
  }
  return width;
}

int Window::height() const noexcept {
  int width = 0;
  int height = 0;
  if (m_window != nullptr) {
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
  }
  return height;
}

int Window::logicalWidth() const noexcept {
  int width = 0;
  int height = 0;
  if (m_window != nullptr) {
    SDL_GetWindowSize(m_window, &width, &height);
  }
  return width;
}

int Window::logicalHeight() const noexcept {
  int width = 0;
  int height = 0;
  if (m_window != nullptr) {
    SDL_GetWindowSize(m_window, &width, &height);
  }
  return height;
}

float Window::displayScale() const noexcept {
  return m_window != nullptr ? SDL_GetWindowDisplayScale(m_window) : 1.0F;
}

SDL_Window* Window::nativeHandle() const noexcept {
  return m_window;
}

} // namespace crab::platform
