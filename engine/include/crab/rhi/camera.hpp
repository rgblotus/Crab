#pragma once

#include <glm/glm.hpp>

namespace crab::rhi {

// A right-handed perspective camera producing Vulkan clip-space matrices:
// depth is in [0, 1] and the Y axis is flipped for Vulkan's top-left viewport.
struct PerspectiveCamera {
  glm::vec3 position{2.6F, 2.2F, 3.8F};
  glm::vec3 target{0.0F, 0.35F, 0.0F};
  glm::vec3 up{0.0F, 1.0F, 0.0F};
  float verticalFovRadians = 0.959931F; // 55 degrees
  float nearPlane = 0.1F;
  float farPlane = 200.0F;

  [[nodiscard]] glm::mat4 view() const noexcept;
  [[nodiscard]] glm::mat4 projection(float aspectRatio) const noexcept;
  [[nodiscard]] glm::mat4 viewProjection(float aspectRatio) const noexcept;
};

// Orthographic camera for 2D/HUD content. The view rectangle is centered at
// `center`; +Y points up in world space and is mapped to the top of the screen.
struct OrthographicCamera {
  float width = 2.0F;
  float height = 2.0F;
  glm::vec2 center{0.0F, 0.0F};

  [[nodiscard]] glm::mat4 viewProjection() const noexcept;
};

} // namespace crab::rhi
