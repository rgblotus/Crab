#include "crab/core/log.hpp"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <system_error>
#include <vector>

namespace crab::core {
namespace {

std::mutex g_logMutex;
std::shared_ptr<spdlog::logger> g_logger;

spdlog::level::level_enum toSpdlog(LogLevel level) noexcept {
  switch (level) {
  case LogLevel::trace:
    return spdlog::level::trace;
  case LogLevel::debug:
    return spdlog::level::debug;
  case LogLevel::info:
    return spdlog::level::info;
  case LogLevel::warn:
    return spdlog::level::warn;
  case LogLevel::error:
    return spdlog::level::err;
  case LogLevel::critical:
    return spdlog::level::critical;
  }
  return spdlog::level::info;
}

std::filesystem::path defaultLogPath() {
  if (const char* stateHome = std::getenv("XDG_STATE_HOME");
      stateHome != nullptr && *stateHome != '\0') {
    return std::filesystem::path(stateHome) / "crab" / "logs" / "crab.log";
  }
  if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".local" / "state" / "crab" / "logs" / "crab.log";
  }
  return std::filesystem::path("crab.log");
}

void writeFallback(LogLevel level, std::string_view category, std::string_view message) {
  std::fprintf(stderr, "[%s] [%.*s] %.*s\n", toString(level), static_cast<int>(category.size()),
               category.data(), static_cast<int>(message.size()), message.data());
}

} // namespace

void initializeLogging(const LogConfig& config) {
  std::lock_guard lock(g_logMutex);

  if (g_logger != nullptr) {
    g_logger->flush();
    spdlog::drop(g_logger->name());
    g_logger.reset();
  }

  std::vector<spdlog::sink_ptr> sinks;

  if (config.console) {
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  }

  if (config.file) {
    const std::filesystem::path path = config.filePath.empty() ? defaultLogPath() : config.filePath;
    std::error_code error;
    if (!path.parent_path().empty()) {
      std::filesystem::create_directories(path.parent_path(), error);
    }
    if (!error) {
      try {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path.string(),
                                                                               8 * 1024 * 1024, 4));
      } catch (const spdlog::spdlog_ex& exception) {
        std::fprintf(stderr, "Crab: unable to open log file '%s': %s\n", path.c_str(),
                     exception.what());
      }
    } else {
      std::fprintf(stderr, "Crab: unable to create log directory '%s': %s\n",
                   path.parent_path().c_str(), error.message().c_str());
    }
  }

  if (sinks.empty()) {
    sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
  }

  g_logger = std::make_shared<spdlog::logger>("crab", sinks.begin(), sinks.end());
  g_logger->set_level(toSpdlog(config.level));
  g_logger->flush_on(spdlog::level::warn);
  g_logger->set_formatter(std::make_unique<spdlog::pattern_formatter>(config.pattern));

  spdlog::set_default_logger(g_logger);
  spdlog::set_level(toSpdlog(config.level));
}

void shutdownLogging() {
  std::lock_guard lock(g_logMutex);
  if (g_logger != nullptr) {
    g_logger->flush();
    spdlog::drop(g_logger->name());
    g_logger.reset();
  }
  spdlog::shutdown();
}

void logMessage(LogLevel level, std::string_view category, std::string_view message) {
  std::shared_ptr<spdlog::logger> logger;
  {
    std::lock_guard lock(g_logMutex);
    logger = g_logger;
  }

  if (logger == nullptr) {
    writeFallback(level, category, message);
    return;
  }

  try {
    logger->log(toSpdlog(level), "[{}] {}", category, message);
  } catch (...) {
    writeFallback(level, category, message);
  }
}

const char* toString(LogLevel level) noexcept {
  switch (level) {
  case LogLevel::trace:
    return "trace";
  case LogLevel::debug:
    return "debug";
  case LogLevel::info:
    return "info";
  case LogLevel::warn:
    return "warn";
  case LogLevel::error:
    return "error";
  case LogLevel::critical:
    return "critical";
  }
  return "unknown";
}

} // namespace crab::core
