#include "crab/platform/platform.hpp"

#include "crab/core/log.hpp"

#include <SDL3/SDL.h>

namespace crab::platform {
namespace {

bool g_initialized = false;

} // namespace

bool initialize() {
  if (g_initialized) {
    return true;
  }

  const SDL_InitFlags flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD;
  if (!SDL_Init(flags)) {
    CRAB_LOG_ERROR("platform", "SDL_Init failed: {}", SDL_GetError());
    return false;
  }

  const int version = SDL_GetVersion();
  CRAB_LOG_INFO("platform", "SDL {}.{}.{} initialized ({})", SDL_VERSIONNUM_MAJOR(version),
                SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version), SDL_GetRevision());

  g_initialized = true;
  return true;
}

void shutdown() {
  if (g_initialized) {
    CRAB_LOG_INFO("platform", "Shutting down SDL");
    SDL_Quit();
    g_initialized = false;
  }
}

bool isInitialized() noexcept {
  return g_initialized;
}

std::string_view name() noexcept {
  return "SDL3";
}

} // namespace crab::platform
