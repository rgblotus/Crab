#include "crab/core/log.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

TEST_CASE("log level names are stable", "[core][log]") {
  CHECK(std::string_view{crab::core::toString(crab::core::LogLevel::trace)} == "trace");
  CHECK(std::string_view{crab::core::toString(crab::core::LogLevel::debug)} == "debug");
  CHECK(std::string_view{crab::core::toString(crab::core::LogLevel::info)} == "info");
  CHECK(std::string_view{crab::core::toString(crab::core::LogLevel::warn)} == "warn");
  CHECK(std::string_view{crab::core::toString(crab::core::LogLevel::error)} == "error");
  CHECK(std::string_view{crab::core::toString(crab::core::LogLevel::critical)} == "critical");
  CHECK(std::string_view{crab::core::toString(static_cast<crab::core::LogLevel>(99))} == "unknown");
}

TEST_CASE("logging can run without console or file sinks", "[core][log]") {
  crab::core::LogConfig config;
  config.console = false;
  config.file = false;

  crab::core::initializeLogging(config);
  CHECK_NOTHROW(crab::core::logMessage(crab::core::LogLevel::info, "test", "hello"));
  crab::core::shutdownLogging();
}
