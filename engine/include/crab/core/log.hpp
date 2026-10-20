#pragma once

#include <spdlog/fmt/fmt.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace crab::core {

enum class LogLevel {
  trace,
  debug,
  info,
  warn,
  error,
  critical,
};

struct LogConfig {
  LogLevel level =
#ifdef NDEBUG
    LogLevel::info;
#else
    LogLevel::debug;
#endif
  bool console = true;
  bool file = true;
  std::filesystem::path filePath{};
  std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v";
};

// Initializes the global logger. Safe to call more than once; the previous
// configuration is replaced. If `file` is true and `filePath` is empty, the
// log is written under $XDG_STATE_HOME/crab/logs (or ~/.local/state/...).
void initializeLogging(const LogConfig& config = {});
void shutdownLogging();

void logMessage(LogLevel level, std::string_view category, std::string_view message);

template <typename... Args>
void log(LogLevel level, std::string_view category, fmt::format_string<Args...> fmtString,
         Args&&... args) {
  logMessage(level, category, fmt::format(fmtString, std::forward<Args>(args)...));
}

[[nodiscard]] const char* toString(LogLevel level) noexcept;

} // namespace crab::core

#define CRAB_LOG_TRACE(category, ...)                                                              \
  ::crab::core::log(::crab::core::LogLevel::trace, category, __VA_ARGS__)
#define CRAB_LOG_DEBUG(category, ...)                                                              \
  ::crab::core::log(::crab::core::LogLevel::debug, category, __VA_ARGS__)
#define CRAB_LOG_INFO(category, ...)                                                               \
  ::crab::core::log(::crab::core::LogLevel::info, category, __VA_ARGS__)
#define CRAB_LOG_WARN(category, ...)                                                               \
  ::crab::core::log(::crab::core::LogLevel::warn, category, __VA_ARGS__)
#define CRAB_LOG_ERROR(category, ...)                                                              \
  ::crab::core::log(::crab::core::LogLevel::error, category, __VA_ARGS__)
#define CRAB_LOG_CRITICAL(category, ...)                                                           \
  ::crab::core::log(::crab::core::LogLevel::critical, category, __VA_ARGS__)
