#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace Crab {

struct PipelineConfig {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule vertModule = VK_NULL_HANDLE;
    VkShaderModule fragModule = VK_NULL_HANDLE;
    VkExtent2D extent{};
    uint32_t vertexStride = 0;
    std::vector<VkVertexInputAttributeDescription> attributes;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    bool depthTest = false;
};

class Pipeline {
public:
    Pipeline() = default;
    ~Pipeline();

    Pipeline(Pipeline&& other) noexcept;
    Pipeline& operator=(Pipeline&& other) noexcept;
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    bool create(VkDevice device, const PipelineConfig& config);
    void destroy(VkDevice device);

    VkPipeline handle() const { return m_pipeline; }
    VkPipelineLayout layout() const { return m_layout; }
    VkDescriptorSetLayout descriptorSetLayout() const { return m_descriptorSetLayout; }

    void bind(VkCommandBuffer cmd) const;

private:
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
};

} // namespace Crab
