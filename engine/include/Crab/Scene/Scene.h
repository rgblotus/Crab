#pragma once

#include <entt/entt.hpp>

namespace Crab {

class Renderer;

class Scene {
public:
    Scene() = default;

    entt::registry& registry() { return m_registry; }
    const entt::registry& registry() const { return m_registry; }

    entt::entity createEntity();
    void destroyEntity(entt::entity entity);

    void update(float dt);
    void render(Renderer& renderer);

private:
    entt::registry m_registry;
};

} // namespace Crab
