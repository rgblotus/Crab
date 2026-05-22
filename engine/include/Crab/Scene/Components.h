#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

namespace Crab {

class RenderMesh;

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // euler angles (pitch, yaw, roll)
    glm::vec3 scale{1.0f};

    glm::mat4 matrix() const;
};

struct MeshComponent {
    std::shared_ptr<RenderMesh> mesh{nullptr};
};

struct RotationComponent {
    glm::vec3 speed{0.0f, 0.0f, 90.0f}; // degrees per second
};

} // namespace Crab
