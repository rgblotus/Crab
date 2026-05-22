#pragma once

#include <fmt/core.h>
#include <string_view>
#include <source_location>

namespace Crab {

enum class LogLevel {
    Trace, Debug, Info, Warn, Error
};

class Logger {
public:
    static void init();
    static void shutdown();
    static void setLevel(LogLevel level);

    template<typename... Args>
    static void trace(fmt::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Trace, fmt::format(fmt, std::forward<Args>(args)...),
            std::source_location::current());
    }

    template<typename... Args>
    static void debug(fmt::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Debug, fmt::format(fmt, std::forward<Args>(args)...),
            std::source_location::current());
    }

    template<typename... Args>
    static void info(fmt::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Info, fmt::format(fmt, std::forward<Args>(args)...),
            std::source_location::current());
    }

    template<typename... Args>
    static void warn(fmt::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Warn, fmt::format(fmt, std::forward<Args>(args)...),
            std::source_location::current());
    }

    template<typename... Args>
    static void error(fmt::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Error, fmt::format(fmt, std::forward<Args>(args)...),
            std::source_location::current());
    }

private:
    static void log(LogLevel level, std::string_view msg, std::source_location loc);
    static LogLevel s_level;
};

} // namespace Crab
