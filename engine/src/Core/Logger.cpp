#include <Crab/Core/Logger.h>
#include <fmt/core.h>
#include <string_view>

namespace Crab {

LogLevel Logger::s_level = LogLevel::Trace;

void Logger::init() {
    fmt::println("[Logger] initialized");
}

void Logger::shutdown() {
    fmt::println("[Logger] shutdown");
}

void Logger::setLevel(LogLevel level) {
    s_level = level;
}

void Logger::log(LogLevel level, std::string_view msg, std::source_location loc) {
    if (level < s_level) return;

    const char* levelStr = "";
    switch (level) {
        case LogLevel::Trace: levelStr = "TRACE"; break;
        case LogLevel::Debug: levelStr = "DEBUG"; break;
        case LogLevel::Info:  levelStr = "INFO";  break;
        case LogLevel::Warn:  levelStr = "WARN";  break;
        case LogLevel::Error: levelStr = "ERROR"; break;
    }

    fmt::println("[{}] {} ({}:{})", levelStr, msg,
                 loc.file_name(), loc.line());
}

} // namespace Crab
