#include <Crab/Render/Renderer.h>
#include <Crab/Render/Shader.h>
#include <Crab/Render/RenderMesh.h>
#include <Crab/Platform/Window.h>
#include <Crab/Core/Logger.h>

namespace Crab {

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(Window& window) {
    m_window = &window;
    if (!m_context.init(window)) return false;
    if (!createRenderPass()) { Logger::error("Failed to create render pass"); return false; }
    if (!createPipeline()) { Logger::error("Failed to create pipeline"); return false; }
    if (!createFramebuffers()) { Logger::error("Failed to create framebuffers"); return false; }
    if (!createSyncObjects()) { Logger::error("Failed to create sync objects"); return false; }
    m_camera.setAspect(window.aspectRatio());
    Logger::info("Renderer initialized");
    return true;
}

void Renderer::shutdown() {
    if (m_context.device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_context.device());
        if (m_inFlightFence) vkDestroyFence(m_context.device(), m_inFlightFence, nullptr);
        if (m_renderFinished) vkDestroySemaphore(m_context.device(), m_renderFinished, nullptr);
        if (m_imageAvailable) vkDestroySemaphore(m_context.device(), m_imageAvailable, nullptr);
        destroyFramebuffers();
        m_pipeline.destroy(m_context.device());
        if (m_renderPass) vkDestroyRenderPass(m_context.device(), m_renderPass, nullptr);
    }
    m_context.shutdown();
    Logger::info("Renderer shut down");
}

bool Renderer::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = m_context.swapchainFormat();
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;

    return vkCreateRenderPass(m_context.device(), &info, nullptr, &m_renderPass) == VK_SUCCESS;
}

bool Renderer::createPipeline() {
    Shader vertShader, fragShader;

    bool loaded = vertShader.loadFromFile(m_context.device(), "shaders/triangle.vert", VK_SHADER_STAGE_VERTEX_BIT) &&
                  fragShader.loadFromFile(m_context.device(), "shaders/triangle.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

    if (!loaded) {
        const std::string vertSrc = R"(
#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 fragColor;
layout(binding = 0) uniform MeshUniform {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}
)";

        const std::string fragSrc = R"(
#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(fragColor, 1.0);
}
)";

        loaded = vertShader.compile(m_context.device(), vertSrc, VK_SHADER_STAGE_VERTEX_BIT) &&
                 fragShader.compile(m_context.device(), fragSrc, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    if (!loaded) return false;

    std::vector<VkVertexInputAttributeDescription> attrs(2);
    attrs[0].binding = 0; attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[0].offset = offsetof(Vertex, pos);
    attrs[1].binding = 0; attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[1].offset = offsetof(Vertex, color);

    PipelineConfig config{};
    config.renderPass = m_renderPass;
    config.vertModule = vertShader.module();
    config.fragModule = fragShader.module();
    config.extent = m_context.swapchainExtent();
    config.vertexStride = sizeof(Vertex);
    config.attributes = attrs;

    bool ok = m_pipeline.create(m_context.device(), config);
    vertShader.destroy(m_context.device());
    fragShader.destroy(m_context.device());
    return ok;
}

bool Renderer::createFramebuffers() {
    const auto& views = m_context.swapchainImageViews();
    m_swapchainFramebuffers.resize(views.size());
    for (size_t i = 0; i < views.size(); ++i) {
        VkImageView attachments[] = {views[i]};
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_renderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachments;
        info.width = m_context.swapchainExtent().width;
        info.height = m_context.swapchainExtent().height;
        info.layers = 1;
        if (vkCreateFramebuffer(m_context.device(), &info, nullptr, &m_swapchainFramebuffers[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

void Renderer::destroyFramebuffers() {
    for (auto fb : m_swapchainFramebuffers)
        vkDestroyFramebuffer(m_context.device(), fb, nullptr);
    m_swapchainFramebuffers.clear();
}

bool Renderer::recreateFramebuffers() {
    destroyFramebuffers();
    return createFramebuffers();
}

bool Renderer::createSyncObjects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    return vkCreateSemaphore(m_context.device(), &semInfo, nullptr, &m_imageAvailable) == VK_SUCCESS &&
           vkCreateSemaphore(m_context.device(), &semInfo, nullptr, &m_renderFinished) == VK_SUCCESS &&
           vkCreateFence(m_context.device(), &fenceInfo, nullptr, &m_inFlightFence) == VK_SUCCESS;
}

bool Renderer::beginFrame() {
    vkWaitForFences(m_context.device(), 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);

    VkResult result = m_context.acquireNextImage(m_imageAvailable, &m_currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!m_context.recreateSwapchain(*m_window)) return false;
        recreateFramebuffers();
        result = m_context.acquireNextImage(m_imageAvailable, &m_currentImageIndex);
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) return false;

    vkResetFences(m_context.device(), 1, &m_inFlightFence);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_context.commandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(m_context.device(), &allocInfo, &m_commandBuffer) != VK_SUCCESS)
        return false;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

    VkClearValue clearColor = {{{0.02f, 0.02f, 0.04f, 1.0f}}};
    VkRenderPassBeginInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderInfo.renderPass = m_renderPass;
    renderInfo.framebuffer = m_swapchainFramebuffers[m_currentImageIndex];
    renderInfo.renderArea.offset = {0, 0};
    renderInfo.renderArea.extent = m_context.swapchainExtent();
    renderInfo.clearValueCount = 1;
    renderInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(m_commandBuffer, &renderInfo, VK_SUBPASS_CONTENTS_INLINE);
    m_pipeline.bind(m_commandBuffer);

    return true;
}

void Renderer::endFrame() {
    vkCmdEndRenderPass(m_commandBuffer);
    vkEndCommandBuffer(m_commandBuffer);

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageAvailable;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinished;

    if (vkQueueSubmit(m_context.graphicsQueue(), 1, &submitInfo, m_inFlightFence) != VK_SUCCESS)
        Logger::error("Failed to submit command buffer");

    VkResult result = m_context.present(m_context.presentQueue(), m_currentImageIndex, m_renderFinished);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        if (!m_context.recreateSwapchain(*m_window))
            Logger::error("Failed to recreate swapchain");
        recreateFramebuffers();
    }

    vkFreeCommandBuffers(m_context.device(), m_context.commandPool(), 1, &m_commandBuffer);
}

void Renderer::drawMesh(RenderMesh& mesh, const glm::mat4& modelMatrix) {
    mesh.updateUniform(m_context, m_currentImageIndex, modelMatrix, m_camera);
    mesh.draw(m_commandBuffer, m_pipeline.layout(), mesh.descriptorSet(m_currentImageIndex));
}

std::unique_ptr<RenderMesh> Renderer::createRenderMesh(const MeshData& data) {
    auto mesh = std::make_unique<RenderMesh>();
    if (!mesh->create(m_context, m_pipeline.descriptorSetLayout(), data)) return nullptr;
    return mesh;
}

} // namespace Crab
