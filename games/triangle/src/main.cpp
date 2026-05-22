#include <Crab/Engine.h>
#include <Crab/Core/Logger.h>

int main() {
    Crab::Engine engine;

    if (!engine.init(800, 600, "Crab Engine - Triangle Demo")) {
        return 1;
    }

    // Create a colorful triangle mesh from the game side
    Crab::MeshData triangle;
    triangle.vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},   // bottom-left : red
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},   // bottom-right: green
        {{ 0.0f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},   // top-center  : blue
    };

    // Convenience: engine creates RenderMesh + ECS entity with
    // TransformComponent + RotationComponent + MeshComponent
    engine.createRenderable(triangle);

    // Set up the camera
    auto& camera = engine.renderer().camera();
    camera.setPosition({0.0f, 0.0f, 2.5f});
    camera.lookAt({0.0f, 0.0f, 0.0f});

    Crab::Logger::info("Running colorful rotating triangle...");
    engine.run();

    Crab::Logger::info("Shutdown complete.");
    return 0;
}
