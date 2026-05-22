#pragma once

#include <glm/glm.hpp>

namespace Crab {

class Camera {
public:
    Camera() = default;

    void setPerspective(float fovDeg, float aspect, float nearZ, float farZ);
    void setPosition(const glm::vec3& pos) { m_position = pos; }
    void lookAt(const glm::vec3& target);

    void orbit(float deltaYaw, float deltaPitch);
    void zoom(float delta);

    glm::mat4 viewMatrix() const;
    glm::mat4 projMatrix() const;

    const glm::vec3& position() const { return m_position; }
    float fov() const { return m_fov; }
    float aspect() const { return m_aspect; }

    void setAspect(float aspect) { m_aspect = aspect; }

private:
    glm::vec3 m_position{0.0f, 0.0f, 2.0f};
    glm::vec3 m_target{0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_distance = 2.0f;

    float m_fov = 45.0f;
    float m_aspect = 16.0f / 9.0f;
    float m_nearZ = 0.1f;
    float m_farZ = 100.0f;
};

} // namespace Crab
