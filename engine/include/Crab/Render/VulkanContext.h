#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace Crab {

class Window;

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    bool init(Window& window);
    void shutdown();
    bool recreateSwapchain(Window& window);

    // Accessors
    VkInstance instance() const { return m_instance; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice device() const { return m_device; }
    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }
    VkCommandPool commandPool() const { return m_commandPool; }

    uint32_t graphicsFamily() const { return m_graphicsFamily; }
    uint32_t presentFamily() const { return m_presentFamily; }

    // Swapchain
    VkSwapchainKHR swapchain() const { return m_swapchain; }
    VkFormat swapchainFormat() const { return m_swapchainFormat; }
    VkExtent2D swapchainExtent() const { return m_swapchainExtent; }
    const std::vector<VkImage>& swapchainImages() const { return m_swapchainImages; }
    const std::vector<VkImageView>& swapchainImageViews() const { return m_swapchainImageViews; }

    // Helpers
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                                  VkImageTiling tiling,
                                  VkFormatFeatureFlags features) const;

    void submitAndWait(VkCommandBuffer cmdBuf) const;
    VkResult acquireNextImage(VkSemaphore semaphore, uint32_t* imageIndex) const;
    VkResult present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) const;

private:
    bool createInstance();
    bool createSurface(Window& window);
    bool pickPhysicalDevice();
    bool createDevice();
    bool createSwapchain();
    bool createCommandPool();
    void destroySwapchain();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsFamily = UINT32_MAX;
    uint32_t m_presentFamily = UINT32_MAX;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat;
    VkExtent2D m_swapchainExtent{};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    static constexpr const char* VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";
};

} // namespace Crab
