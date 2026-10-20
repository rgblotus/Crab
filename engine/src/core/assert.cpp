#include "crab/core/assert.hpp"

#include "crab/core/log.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace crab::core {
namespace {

std::string formatLocation(std::string_view expression, const std::source_location& location) {
  return fmt::format("{} ({}:{} in {})", expression, location.file_name(), location.line(),
                     location.function_name());
}

} // namespace

void debugBreak() noexcept {
#if defined(_MSC_VER)
  __debugbreak();
#else
  std::raise(SIGTRAP);
#endif
}

void assertionFailed(std::string_view expression, const std::source_location& location) {
  logMessage(LogLevel::critical, "assert",
             fmt::format("Assertion failed: {}", formatLocation(expression, location)));
  std::fflush(stderr);
  debugBreak();
  std::abort();
}

void fatalError(std::string_view message, const std::source_location& location) {
  logMessage(LogLevel::critical, "fatal",
             fmt::format("Fatal error: {}", formatLocation(message, location)));
  std::fflush(stderr);
  debugBreak();
  std::abort();
}

} // namespace crab::core
