#pragma once

#include <Crab/Render/VulkanContext.h>
#include <Crab/Render/Pipeline.h>
#include <Crab/Render/Camera.h>
#include <Crab/Core/Types.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace Crab {

class Window;
class RenderMesh;

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(Window& window);
    void shutdown();

    // Frame control
    bool beginFrame();
    void endFrame();
    void drawMesh(RenderMesh& mesh, const glm::mat4& modelMatrix);

    // Access
    VulkanContext& context() { return m_context; }
    Camera& camera() { return m_camera; }
    const Camera& camera() const { return m_camera; }
    VkDescriptorSetLayout descriptorSetLayout() const { return m_pipeline.descriptorSetLayout(); }

    // Mesh creation
    std::unique_ptr<RenderMesh> createRenderMesh(const MeshData& data);

private:
    bool createRenderPass();
    bool createPipeline();
    bool createFramebuffers();
    bool createSyncObjects();
    void destroyFramebuffers();
    bool recreateFramebuffers();

    VulkanContext m_context;
    Pipeline m_pipeline;
    Camera m_camera;
    Window* m_window = nullptr;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_swapchainFramebuffers;

    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkSemaphore m_imageAvailable{};
    VkSemaphore m_renderFinished{};
    VkFence m_inFlightFence{};

    uint32_t m_currentImageIndex = 0;
};

} // namespace Crab
