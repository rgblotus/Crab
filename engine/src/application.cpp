#include "crab/application.hpp"

#include "crab/core/log.hpp"
#include "crab/core/version.hpp"
#include "crab/platform/platform.hpp"
#include "crab/platform/window.hpp"
#include "crab/rhi/renderer.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdlib>
#include <thread>
#include <utility>

namespace crab {
namespace {

using Clock = std::chrono::steady_clock;

double nowSeconds() {
  return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

} // namespace

Application::Application(ApplicationConfig config) : m_config(std::move(config)) {}

Application::~Application() {
  shutdown();
}

bool Application::init() {
  if (m_initialized) {
    return true;
  }

  if (m_config.width <= 0) {
    m_config.width = 1280;
  }
  if (m_config.height <= 0) {
    m_config.height = 720;
  }
  if (m_config.targetFrameRate <= 0.0) {
    m_config.targetFrameRate = 60.0;
  }

  CRAB_LOG_INFO("app", "Starting {} {} ({}, {})", core::kEngineName, core::kEngineVersion,
                core::kBuildType, core::kCompiler);

  if (!m_config.headless) {
    if (!platform::initialize()) {
      return false;
    }

    platform::WindowConfig windowConfig;
    windowConfig.title = m_config.title;
    windowConfig.width = m_config.width;
    windowConfig.height = m_config.height;
    windowConfig.resizable = m_config.resizable;
    windowConfig.highPixelDensity = m_config.highPixelDensity;
    windowConfig.vulkan = m_config.vulkan;

    m_window = platform::Window::create(windowConfig);
    if (m_window == nullptr) {
      platform::shutdown();
      return false;
    }

    CRAB_LOG_INFO("app", "Window ready: {}x{} pixels, {}x{} logical, scale {:.2f}",
                  m_window->width(), m_window->height(), m_window->logicalWidth(),
                  m_window->logicalHeight(), static_cast<double>(m_window->displayScale()));
  } else {
    CRAB_LOG_INFO("app", "Headless mode: SDL video is disabled");
  }

  if (m_config.vulkan) {
    m_renderer = std::make_unique<rhi::Renderer>();
    const bool rendererReady = m_config.headless
                                 ? m_renderer->initializeHeadless(m_config.width, m_config.height)
                                 : m_renderer->initialize(*m_window);

    if (!rendererReady) {
      CRAB_LOG_ERROR("app", "Renderer initialization failed: {}", m_renderer->lastError());
      m_renderer->shutdown();
      m_renderer.reset();
      m_window.reset();
      if (!m_config.headless) {
        platform::shutdown();
      }
      return false;
    }

    const rhi::RendererStats rendererStats = m_renderer->stats();
    CRAB_LOG_INFO("app", "Renderer ready: {} ({}x{}, {} frames in flight)",
                  rendererStats.deviceName.empty() ? "unknown device" : rendererStats.deviceName,
                  rendererStats.swapchainWidth, rendererStats.swapchainHeight,
                  rendererStats.framesInFlight);
  } else {
    CRAB_LOG_INFO("app", "Vulkan rendering disabled by configuration");
  }

  m_startTime = nowSeconds();
  m_frameCount = 0;
  m_running = true;
  m_initialized = true;
  return true;
}

int Application::run() {
  if (!m_initialized) {
    CRAB_LOG_ERROR("app", "Application::run() called before a successful init()");
    return EXIT_FAILURE;
  }

  CRAB_LOG_INFO("app", "Entering main loop ({} Hz target, max frames: {})",
                m_config.targetFrameRate,
                m_config.maxFrames == 0 ? "unbounded" : std::to_string(m_config.maxFrames));

  const double frameBudget = 1.0 / m_config.targetFrameRate;
  int exitCode = EXIT_SUCCESS;

  while (m_running) {
    const Clock::time_point frameStart = Clock::now();

    processEvents();

    if (!m_running) {
      break;
    }

    if (m_config.maxFrames > 0 && m_frameCount >= m_config.maxFrames) {
      break;
    }

    // Phase 2 renders a full-screen textured triangle and the debug overlay.
    // Simulation, fixed-step updates, and audio will be added in later phases.
    if (m_renderer != nullptr && m_renderer->isInitialized()) {
      if (m_renderer->beginFrame()) {
        rhi::OverlayStats overlayStats;
        overlayStats.frameTimeMs = m_frameTimeMs;
        overlayStats.framesPerSecond = m_framesPerSecond;
        overlayStats.frameIndex = m_frameCount;
        overlayStats.deviceName = m_renderer->stats().deviceName;
        m_renderer->drawDebugOverlay(overlayStats);

        if (!m_renderer->endFrame()) {
          CRAB_LOG_ERROR("app", "Renderer failed while submitting frame {}: {}", m_frameCount,
                         m_renderer->lastError());
          exitCode = EXIT_FAILURE;
          m_running = false;
          break;
        }
      } else if (m_renderer->isFrameAborted()) {
        CRAB_LOG_ERROR("app", "Renderer aborted at frame {}: {}", m_frameCount,
                       m_renderer->lastError());
        exitCode = EXIT_FAILURE;
        m_running = false;
        break;
      }
    }

    ++m_frameCount;

    const Clock::time_point frameEnd = Clock::now();
    m_frameTimeMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
    m_framesPerSecond = m_frameTimeMs > 0.0 ? 1000.0 / m_frameTimeMs : 0.0;

    if (!m_config.headless) {
      const double elapsed = std::chrono::duration<double>(frameEnd - frameStart).count();
      if (elapsed < frameBudget) {
        std::this_thread::sleep_for(std::chrono::duration<double>(frameBudget - elapsed));
      }
    }
  }

  m_running = false;
  CRAB_LOG_INFO("app", "Main loop stopped after {} frames ({:.3f}s)", m_frameCount,
                elapsedSeconds());
  return exitCode;
}

void Application::shutdown() {
  if (!m_initialized) {
    return;
  }

  CRAB_LOG_INFO("app", "Shutting down after {} frames", m_frameCount);
  m_running = false;
  if (m_renderer != nullptr) {
    m_renderer->shutdown();
    m_renderer.reset();
  }
  m_window.reset();
  if (!m_config.headless) {
    platform::shutdown();
  }
  m_initialized = false;
}

void Application::requestExit() noexcept {
  m_running = false;
}

bool Application::isRunning() const noexcept {
  return m_running;
}

std::uint64_t Application::frameCount() const noexcept {
  return m_frameCount;
}

double Application::elapsedSeconds() const noexcept {
  return m_initialized ? nowSeconds() - m_startTime : 0.0;
}

platform::Window* Application::window() noexcept {
  return m_window.get();
}

rhi::Renderer* Application::renderer() noexcept {
  return m_renderer.get();
}

void Application::processEvents() {
  if (m_window == nullptr) {
    return;
  }

  const SDL_WindowID windowId = SDL_GetWindowID(m_window->nativeHandle());
  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    if (m_renderer != nullptr) {
      m_renderer->processEvent(event);
    }

    switch (event.type) {
    case SDL_EVENT_QUIT:
      m_running = false;
      break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      if (event.window.windowID == windowId) {
        m_window->requestClose();
        m_running = false;
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      if (!event.key.repeat && event.key.key == SDLK_ESCAPE) {
        m_running = false;
      }
      break;
    case SDL_EVENT_WINDOW_RESIZED:
      CRAB_LOG_DEBUG("window", "Logical size changed to {}x{}", event.window.data1,
                     event.window.data2);
      break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      CRAB_LOG_DEBUG("window", "Pixel size changed to {}x{}", event.window.data1,
                     event.window.data2);
      if (m_renderer != nullptr) {
        m_renderer->requestResize(event.window.data1, event.window.data2);
      }
      break;
    default:
      break;
    }
  }
}

} // namespace crab
