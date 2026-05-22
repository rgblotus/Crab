#include <Crab/Scene/Scene.h>
#include <Crab/Scene/Components.h>
#include <Crab/Render/Renderer.h>
#include <Crab/Render/RenderMesh.h>

namespace Crab {

entt::entity Scene::createEntity() {
    return m_registry.create();
}

void Scene::destroyEntity(entt::entity entity) {
    m_registry.destroy(entity);
}

void Scene::update(float dt) {
    auto view = m_registry.view<TransformComponent, RotationComponent>();
    for (auto entity : view) {
        auto& trans = view.get<TransformComponent>(entity);
        auto& rot = view.get<RotationComponent>(entity);
        trans.rotation += rot.speed * dt;
    }
}

void Scene::render(Renderer& renderer) {
    auto meshView = m_registry.view<TransformComponent, MeshComponent>();
    for (auto entity : meshView) {
        auto& trans = meshView.get<TransformComponent>(entity);
        auto& meshComp = meshView.get<MeshComponent>(entity);
        if (meshComp.mesh) {
            renderer.drawMesh(*meshComp.mesh, trans.matrix());
        }
    }
}

} // namespace Crab
