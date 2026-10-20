#pragma once

#include <string_view>

namespace crab::platform {

// Initializes SDL video, events, and gamepad support. Safe to call once during
// application startup; Window::create requires this to have succeeded.
bool initialize();
void shutdown();

[[nodiscard]] bool isInitialized() noexcept;
[[nodiscard]] std::string_view name() noexcept;

} // namespace crab::platform
