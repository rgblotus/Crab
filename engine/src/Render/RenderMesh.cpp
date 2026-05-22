#include <Crab/Render/RenderMesh.h>
#include <Crab/Render/VulkanContext.h>
#include <Crab/Render/Camera.h>
#include <cstring>

namespace Crab {

RenderMesh::RenderMesh(RenderMesh&& other) noexcept
    : m_vertexBuffer(std::move(other.m_vertexBuffer))
    , m_uniformBuffers(std::move(other.m_uniformBuffers))
    , m_descriptorSets(std::move(other.m_descriptorSets))
    , m_descriptorPool(other.m_descriptorPool)
    , m_vertexCount(other.m_vertexCount)
{
    other.m_descriptorPool = VK_NULL_HANDLE;
    other.m_vertexCount = 0;
}

RenderMesh& RenderMesh::operator=(RenderMesh&& other) noexcept {
    if (this != &other) {
        m_vertexBuffer = std::move(other.m_vertexBuffer);
        m_uniformBuffers = std::move(other.m_uniformBuffers);
        m_descriptorSets = std::move(other.m_descriptorSets);
        m_descriptorPool = other.m_descriptorPool;
        m_vertexCount = other.m_vertexCount;
        other.m_descriptorPool = VK_NULL_HANDLE;
        other.m_vertexCount = 0;
    }
    return *this;
}

RenderMesh::~RenderMesh() {
    // Must call destroy() explicitly
}

bool RenderMesh::create(const VulkanContext& ctx, VkDescriptorSetLayout descLayout,
                         const MeshData& data)
{
    VkDevice device = ctx.device();
    m_vertexCount = static_cast<uint32_t>(data.vertices.size());

    // --- Vertex buffer ---
    VkDeviceSize vbSize = m_vertexCount * sizeof(Vertex);
    if (!m_vertexBuffer.create(ctx, vbSize,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                data.vertices.data()))
    {
        return false;
    }

    // --- Uniform buffers (one per frame in flight, per swapchain image) ---
    // We use the swapchain image count as "frames in flight"
    uint32_t imageCount = static_cast<uint32_t>(ctx.swapchainImages().size());
    m_uniformBuffers.resize(imageCount);
    m_descriptorSets.resize(imageCount);

    VkDeviceSize uboSize = sizeof(MeshUniform);
    for (uint32_t i = 0; i < imageCount; ++i) {
        if (!m_uniformBuffers[i].create(ctx, uboSize,
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            return false;
        }
    }

    // --- Descriptor pool ---
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = imageCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = imageCount;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        return false;
    }

    // --- Descriptor sets ---
    std::vector<VkDescriptorSetLayout> layouts(imageCount, descLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = imageCount;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        return false;
    }

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_uniformBuffers[i].handle();
        bufInfo.offset = 0;
        bufInfo.range = sizeof(MeshUniform);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_descriptorSets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    return true;
}

void RenderMesh::destroy(VkDevice device) {
    if (m_descriptorPool) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    m_descriptorSets.clear();
    for (auto& ub : m_uniformBuffers) {
        ub.destroy(device);
    }
    m_uniformBuffers.clear();
    m_vertexBuffer.destroy(device);
    m_vertexCount = 0;
}

void RenderMesh::draw(VkCommandBuffer cmd, VkPipelineLayout layout,
                       VkDescriptorSet descriptorSet) const
{
    VkBuffer vertexBuffers[] = {m_vertexBuffer.handle()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDraw(cmd, m_vertexCount, 1, 0, 0);
}

void RenderMesh::updateUniform(const VulkanContext& ctx, uint32_t imageIndex,
                                const glm::mat4& model, const Camera& camera)
{
    MeshUniform ubo{};
    ubo.model = model;
    ubo.view = camera.viewMatrix();
    ubo.proj = camera.projMatrix();

    if (imageIndex < m_uniformBuffers.size()) {
        std::memcpy(m_uniformBuffers[imageIndex].map(ctx.device()),
                    &ubo, sizeof(ubo));
        m_uniformBuffers[imageIndex].unmap(ctx.device());
    }
}

} // namespace Crab
