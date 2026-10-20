#include "crab/rhi/camera.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

TEST_CASE("perspective camera maps its target to the center of Vulkan NDC", "[rhi][camera]") {
  crab::rhi::PerspectiveCamera camera;
  const glm::mat4 viewProjection = camera.viewProjection(16.0F / 9.0F);
  const glm::vec4 clip = viewProjection * glm::vec4(camera.target, 1.0F);
  REQUIRE(clip.w > 0.0F);
  const glm::vec3 ndc = glm::vec3(clip) / clip.w;

  CHECK(ndc.x == Catch::Approx(0.0F).margin(0.0001F));
  CHECK(ndc.y == Catch::Approx(0.0F).margin(0.0001F));
  CHECK(ndc.z > 0.0F);
  CHECK(ndc.z < 1.0F);
}

TEST_CASE("orthographic camera maps +Y to the top of the screen", "[rhi][camera]") {
  crab::rhi::OrthographicCamera camera;
  camera.width = 2.0F;
  camera.height = 2.0F;

  const glm::mat4 viewProjection = camera.viewProjection();
  const glm::vec4 topClip = viewProjection * glm::vec4(0.0F, 1.0F, 0.0F, 1.0F);
  const glm::vec4 bottomClip = viewProjection * glm::vec4(0.0F, -1.0F, 0.0F, 1.0F);

  CHECK(topClip.y == Catch::Approx(-1.0F).margin(0.0001F));
  CHECK(bottomClip.y == Catch::Approx(1.0F).margin(0.0001F));
}
