#include "crab/application.hpp"

#include "crab/core/log.hpp"
#include "crab/rhi/renderer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

TEST_CASE("headless application runs a bounded number of frames", "[application]") {
  crab::core::LogConfig logConfig;
  logConfig.console = false;
  logConfig.file = false;
  crab::core::initializeLogging(logConfig);

  crab::ApplicationConfig applicationConfig;
  applicationConfig.headless = true;
  applicationConfig.maxFrames = 3;
  applicationConfig.targetFrameRate = 1000.0;

  crab::Application application(applicationConfig);
  REQUIRE(application.init());
  CHECK(application.run() == EXIT_SUCCESS);
  CHECK(application.frameCount() == 3);
  CHECK(application.elapsedSeconds() >= 0.0);
  REQUIRE(application.renderer() != nullptr);
  CHECK_FALSE(application.renderer()->isFrameAborted());
  CHECK(application.renderer()->stats().validationErrors == 0);

  application.shutdown();
  crab::core::shutdownLogging();
}
