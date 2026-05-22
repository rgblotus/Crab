#pragma once

#include <Crab/Core/Types.h>
#include <Crab/Core/Logger.h>
#include <Crab/Platform/Window.h>
#include <Crab/Platform/Input.h>
#include <Crab/Scene/Scene.h>
#include <Crab/Scene/Components.h>
#include <Crab/Render/Renderer.h>
#include <Crab/Render/Camera.h>
#include <Crab/Render/RenderMesh.h>

#include <memory>

namespace Crab {

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool init(int width, int height, const char* title);
    void shutdown();
    void run();

    // Subsystem access
    Window& window() { return *m_window; }
    Input& input() { return m_input; }
    Renderer& renderer() { return *m_renderer; }
    Scene& scene() { return m_scene; }

    // Convenience: create a mesh and add to an entity
    entt::entity createRenderable(const MeshData& data);

private:
    std::unique_ptr<Window> m_window;
    Input m_input;
    std::unique_ptr<Renderer> m_renderer;
    Scene m_scene;
    bool m_running = false;
};

} // namespace Crab
