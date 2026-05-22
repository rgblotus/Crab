#include <Crab/Render/Camera.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Crab {

void Camera::setPerspective(float fovDeg, float aspect, float nearZ, float farZ) {
    m_fov = fovDeg;
    m_aspect = aspect;
    m_nearZ = nearZ;
    m_farZ = farZ;
}

void Camera::lookAt(const glm::vec3& target) {
    m_target = target;
    glm::vec3 dir = glm::normalize(target - m_position);
    m_yaw = glm::degrees(std::atan2(dir.z, dir.x));
    m_pitch = glm::degrees(std::asin(dir.y));
    m_distance = glm::length(target - m_position);
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(m_position, m_target, m_up);
}

glm::mat4 Camera::projMatrix() const {
    glm::mat4 proj = glm::perspective(glm::radians(m_fov), m_aspect, m_nearZ, m_farZ);
    proj[1][1] *= -1; // Vulkan Y-flip
    return proj;
}

void Camera::orbit(float deltaYaw, float deltaPitch) {
    m_yaw += deltaYaw;
    m_pitch = glm::clamp(m_pitch + deltaPitch, -89.0f, 89.0f);

    glm::vec3 dir;
    dir.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    dir.y = std::sin(glm::radians(m_pitch));
    dir.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));

    m_position = m_target - dir * m_distance;
}

void Camera::zoom(float delta) {
    m_distance = glm::clamp(m_distance - delta, 0.1f, 100.0f);
    orbit(0.0f, 0.0f); // Recalculate position
}

} // namespace Crab
