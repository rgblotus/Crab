#pragma once

#include <source_location>
#include <string_view>

namespace crab::core {

[[noreturn]] void
assertionFailed(std::string_view expression,
                const std::source_location& location = std::source_location::current());

[[noreturn]] void
fatalError(std::string_view message,
           const std::source_location& location = std::source_location::current());

void debugBreak() noexcept;

} // namespace crab::core

#if !defined(NDEBUG)
#define CRAB_ASSERT(expression)                                                                    \
  do {                                                                                             \
    if (!(expression)) {                                                                           \
      ::crab::core::assertionFailed(#expression, std::source_location::current());                 \
    }                                                                                              \
  } while (false)
#else
#define CRAB_ASSERT(expression) ((void)0)
#endif

#define CRAB_VERIFY(expression)                                                                    \
  do {                                                                                             \
    if (!(expression)) {                                                                           \
      ::crab::core::assertionFailed(#expression, std::source_location::current());                 \
    }                                                                                              \
  } while (false)

#define CRAB_FATAL(message) ::crab::core::fatalError((message), std::source_location::current())

#define CRAB_UNREACHABLE()                                                                         \
  ::crab::core::assertionFailed("unreachable", std::source_location::current())
