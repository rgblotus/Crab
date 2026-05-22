#pragma once

#include <Crab/Core/Types.h>
#include <Crab/Render/GPUBuffer.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Crab {

class VulkanContext;
class Camera;

struct MeshUniform {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

class RenderMesh {
public:
    RenderMesh() = default;
    ~RenderMesh();

    RenderMesh(RenderMesh&& other) noexcept;
    RenderMesh& operator=(RenderMesh&& other) noexcept;
    RenderMesh(const RenderMesh&) = delete;
    RenderMesh& operator=(const RenderMesh&) = delete;

    bool create(const VulkanContext& ctx, VkDescriptorSetLayout descLayout,
                const MeshData& data);
    void destroy(VkDevice device);

    void draw(VkCommandBuffer cmd, VkPipelineLayout layout,
              VkDescriptorSet descriptorSet) const;

    // Per-frame update: writes model/view/proj to uniform buffer
    void updateUniform(const VulkanContext& ctx, uint32_t imageIndex,
                       const glm::mat4& model, const Camera& camera);

    // Descriptor set for this mesh's uniform buffer
    VkDescriptorSet descriptorSet(uint32_t imageIndex) const {
        return m_descriptorSets[imageIndex];
    }

    uint32_t vertexCount() const { return m_vertexCount; }

private:
    GPUBuffer m_vertexBuffer;
    std::vector<GPUBuffer> m_uniformBuffers;
    std::vector<VkDescriptorSet> m_descriptorSets;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    uint32_t m_vertexCount = 0;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};

} // namespace Crab
