#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Crab {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

struct MeshData {
    std::vector<Vertex> vertices;
};

struct BoundingBox {
    glm::vec3 min{-1.0f};
    glm::vec3 max{1.0f};
};

} // namespace Crab
