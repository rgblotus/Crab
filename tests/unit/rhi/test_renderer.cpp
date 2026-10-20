#include "crab/core/log.hpp"
#include "crab/rhi/renderer.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("headless renderer submits frames and renders the debug overlay", "[rhi]") {
  crab::core::LogConfig logConfig;
  logConfig.console = false;
  logConfig.file = false;
  crab::core::initializeLogging(logConfig);

  crab::rhi::Renderer renderer;
  crab::rhi::RendererConfig rendererConfig;
  rendererConfig.enableImGui = true;
  rendererConfig.validation = true;

  REQUIRE(renderer.initializeHeadless(160, 120, rendererConfig));
  CHECK(renderer.isInitialized());
  CHECK(renderer.isHeadless());

  const crab::rhi::RendererStats initialStats = renderer.stats();
  CHECK(initialStats.headless);
  CHECK(initialStats.imgui);
  CHECK(initialStats.swapchainWidth == 160);
  CHECK(initialStats.swapchainHeight == 120);
  CHECK(initialStats.framesInFlight == 2);
  CHECK(initialStats.validationErrors == 0);

  crab::rhi::OverlayStats overlayStats;
  overlayStats.frameTimeMs = 16.67;
  overlayStats.framesPerSecond = 60.0;

  REQUIRE(renderer.beginFrame());
  renderer.drawDebugOverlay(overlayStats);
  REQUIRE(renderer.endFrame());

  const crab::rhi::RendererStats renderedStats = renderer.stats();
  CHECK(renderedStats.frameIndex == 1);
  CHECK(renderedStats.drawCalls >= 4);
  CHECK(renderedStats.triangles >= 15);
  CHECK(renderedStats.lines >= 22);
  CHECK(renderedStats.renderPasses >= 4);
  CHECK(renderedStats.validationErrors == 0);

  renderer.shutdown();
  CHECK_FALSE(renderer.isInitialized());
  crab::core::shutdownLogging();
}
