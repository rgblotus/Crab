#include "crab/rhi/camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace crab::rhi {

glm::mat4 PerspectiveCamera::view() const noexcept {
  return glm::lookAt(position, target, up);
}

glm::mat4 PerspectiveCamera::projection(float aspectRatio) const noexcept {
  const float safeAspect = std::max(aspectRatio, 0.0001F);
  glm::mat4 projection = glm::perspectiveRH_ZO(verticalFovRadians, safeAspect, nearPlane, farPlane);
  // Vulkan's NDC Y axis points down. Flip the projection so world-space +Y
  // continues to point up on screen.
  projection[1][1] *= -1.0F;
  return projection;
}

glm::mat4 PerspectiveCamera::viewProjection(float aspectRatio) const noexcept {
  return projection(aspectRatio) * view();
}

glm::mat4 OrthographicCamera::viewProjection() const noexcept {
  const float halfWidth = std::max(width, 0.0001F) * 0.5F;
  const float halfHeight = std::max(height, 0.0001F) * 0.5F;
  glm::mat4 projection = glm::ortho(center.x - halfWidth, center.x + halfWidth,
                                    center.y - halfHeight, center.y + halfHeight, -1.0F, 1.0F);
  projection[1][1] *= -1.0F;
  return projection;
}

} // namespace crab::rhi
