#include <Crab/Engine.h>
#include <Crab/Core/Logger.h>
#include <SDL3/SDL.h>
#include <chrono>

namespace Crab {

Engine::Engine() = default;

Engine::~Engine() { shutdown(); }

bool Engine::init(int width, int height, const char* title) {
    Logger::init();

    m_window = std::make_unique<Window>();
    if (!m_window->create(width, height, title)) {
        Logger::error("Failed to create window");
        return false;
    }

    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->init(*m_window)) {
        Logger::error("Failed to initialize renderer");
        return false;
    }

    Logger::info("Engine initialized");
    return true;
}

void Engine::shutdown() {
    m_running = false;
    if (m_renderer) {
        m_renderer->shutdown();
        m_renderer.reset();
    }
    if (m_window) {
        m_window->destroy();
        m_window.reset();
    }
    Logger::shutdown();
}

void Engine::run() {
    m_running = true;
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        m_input.beginFrame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            m_input.handleEvent(event);
            if (event.type == SDL_EVENT_QUIT) {
                m_running = false;
            }
        }

        if (!m_running) break;

        m_scene.update(dt);

        if (m_renderer->beginFrame()) {
            m_scene.render(*m_renderer);
            m_renderer->endFrame();
        }
    }
}

entt::entity Engine::createRenderable(const MeshData& data) {
    auto mesh = m_renderer->createRenderMesh(data);
    if (!mesh) {
        Logger::error("Failed to create render mesh");
        return entt::null;
    }

    auto entity = m_scene.createEntity();
    m_scene.registry().emplace<TransformComponent>(entity);
    m_scene.registry().emplace<RotationComponent>(entity);
    m_scene.registry().emplace<MeshComponent>(entity, std::shared_ptr<RenderMesh>(mesh.release()));
    return entity;
}

} // namespace Crab
