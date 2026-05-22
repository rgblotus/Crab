#include <Crab/Render/VulkanContext.h>
#include <Crab/Core/Logger.h>
#include <Crab/Platform/Window.h>
#include <SDL3/SDL_vulkan.h>
#include <set>
#include <cstring>
#include <algorithm>
#include <limits>

namespace Crab {

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        Logger::error("VK VALIDATION: {}", data->pMessage);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        Logger::warn("VK VALIDATION: {}", data->pMessage);
    else
        Logger::info("VK VALIDATION: {}", data->pMessage);
    return VK_FALSE;
}

// --- Queue family helpers ---

struct QueueFamilyIndices {
    uint32_t graphics = UINT32_MAX;
    uint32_t present = UINT32_MAX;
    bool isComplete() const { return graphics != UINT32_MAX && present != UINT32_MAX; }
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.present = i;
        }
        if (indices.isComplete()) break;
    }
    return indices;
}

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

static SwapchainSupport querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fmtCount, nullptr);
    if (fmtCount > 0) {
        support.formats.resize(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fmtCount, support.formats.data());
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, nullptr);
    if (modeCount > 0) {
        support.presentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, support.presentModes.data());
    }
    return support;
}

static VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto& m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, int w, int h) {
    if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.width != 0)
        return caps.currentExtent;
    VkExtent2D e{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    e.width = std::clamp(e.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    e.height = std::clamp(e.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return e;
}

// --- VulkanContext ---

VulkanContext::~VulkanContext() { shutdown(); }

bool VulkanContext::init(Window& window) {
    if (!createInstance()) { Logger::error("Failed to create Vulkan instance"); return false; }
    if (!createSurface(window)) { Logger::error("Failed to create surface"); return false; }
    if (!pickPhysicalDevice()) { Logger::error("Failed to pick physical device"); return false; }
    if (!createDevice()) { Logger::error("Failed to create device"); return false; }
    if (!createSwapchain()) { Logger::error("Failed to create swapchain"); return false; }
    if (!createCommandPool()) { Logger::error("Failed to create command pool"); return false; }
    Logger::info("Vulkan context initialized");
    return true;
}

void VulkanContext::shutdown() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
        if (m_commandPool) vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        destroySwapchain();
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_surface && m_instance) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (m_debugMessenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }
    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
    Logger::info("Vulkan context shut down");
}

bool VulkanContext::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Crab Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.pEngineName = "Crab Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    Uint32 sdlExtCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    std::vector<const char*> extensions(sdlExts, sdlExts + sdlExtCount);

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    bool hasValidation = false;
    for (const auto& l : availableLayers) {
        if (std::strcmp(l.layerName, VALIDATION_LAYER) == 0) { hasValidation = true; break; }
    }

    bool debugUtilsAvail = false;
    if (hasValidation) {
        uint32_t extCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availExts(extCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availExts.data());
        for (const auto& e : availExts) {
            if (std::strcmp(e.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                debugUtilsAvail = true;
                break;
            }
        }
    }

    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    if (hasValidation && debugUtilsAvail) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = debugCallback;
    }

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();

    if (hasValidation) {
        const char* layer = VALIDATION_LAYER;
        info.enabledLayerCount = 1;
        info.ppEnabledLayerNames = &layer;
        if (debugUtilsAvail) {
            info.pNext = &debugInfo;
            Logger::info("Vulkan validation layers enabled");
        }
    }

    if (vkCreateInstance(&info, nullptr, &m_instance) != VK_SUCCESS) return false;

    if (hasValidation && debugUtilsAvail) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (func) {
            func(m_instance, &debugInfo, nullptr, &m_debugMessenger);
        }
    }

    Logger::info("Vulkan instance created");
    return true;
}

bool VulkanContext::createSurface(Window& window) {
    m_surface = window.createSurface(m_instance);
    return m_surface != VK_NULL_HANDLE;
}

bool VulkanContext::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) return false;

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        QueueFamilyIndices indices = findQueueFamilies(device, m_surface);
        SwapchainSupport support = querySwapchainSupport(device, m_surface);

        bool swapExtSupported = false;
        {
            uint32_t extCount = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
            std::vector<VkExtensionProperties> exts(extCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, exts.data());
            for (const auto& e : exts) {
                if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                    swapExtSupported = true; break;
                }
            }
        }

        if (indices.isComplete() && swapExtSupported &&
            !support.formats.empty() && !support.presentModes.empty()) {
            m_physicalDevice = device;
            m_graphicsFamily = indices.graphics;
            m_presentFamily = indices.present;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) break;
        }
    }
    if (m_physicalDevice == VK_NULL_HANDLE) return false;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    Logger::info("Selected GPU: {}", props.deviceName);
    return true;
}

bool VulkanContext::createDevice() {
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> uniqueFamilies = {m_graphicsFamily, m_presentFamily};
    float priority = 1.0f;

    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qInfo{};
        qInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qInfo.queueFamilyIndex = family;
        qInfo.queueCount = 1;
        qInfo.pQueuePriorities = &priority;
        queueInfos.push_back(qInfo);
    }

    VkPhysicalDeviceFeatures features{};
    const char* swapExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    info.pQueueCreateInfos = queueInfos.data();
    info.pEnabledFeatures = &features;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = &swapExt;

    if (vkCreateDevice(m_physicalDevice, &info, nullptr, &m_device) != VK_SUCCESS) return false;

    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamily, 0, &m_presentQueue);
    Logger::info("Logical device created");
    return true;
}

bool VulkanContext::createSwapchain() {
    SwapchainSupport support = querySwapchainSupport(m_physicalDevice, m_surface);
    VkSurfaceFormatKHR format = chooseFormat(support.formats);
    VkPresentModeKHR mode = choosePresentMode(support.presentModes);

    VkExtent2D extent = chooseExtent(support.capabilities, 800, 600);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        imageCount = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_surface;
    info.minImageCount = imageCount;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (m_graphicsFamily != m_presentFamily) {
        uint32_t families[] = {m_graphicsFamily, m_presentFamily};
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = families;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = mode;
    info.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_device, &info, nullptr, &m_swapchain) != VK_SUCCESS) return false;

    m_swapchainFormat = format.format;
    m_swapchainExtent = extent;

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());

    m_swapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchainFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS)
            return false;
    }

    Logger::info("Swapchain created ({} images, {}x{})", imageCount, extent.width, extent.height);
    return true;
}

bool VulkanContext::createCommandPool() {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = m_graphicsFamily;
    return vkCreateCommandPool(m_device, &info, nullptr, &m_commandPool) == VK_SUCCESS;
}

void VulkanContext::destroySwapchain() {
    for (auto iv : m_swapchainImageViews) vkDestroyImageView(m_device, iv, nullptr);
    m_swapchainImageViews.clear();
    m_swapchainImages.clear();
    if (m_swapchain) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

bool VulkanContext::recreateSwapchain(Window& window) {
    vkDeviceWaitIdle(m_device);

    int w = window.width(), h = window.height();
    while (w == 0 || h == 0) { /* spin */ }

    SwapchainSupport support = querySwapchainSupport(m_physicalDevice, m_surface);
    VkSurfaceFormatKHR format = chooseFormat(support.formats);
    VkPresentModeKHR mode = choosePresentMode(support.presentModes);
    VkExtent2D extent = chooseExtent(support.capabilities, w, h);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        imageCount = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_surface;
    info.minImageCount = imageCount;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (m_graphicsFamily != m_presentFamily) {
        uint32_t families[] = {m_graphicsFamily, m_presentFamily};
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = families;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = m_swapchain;

    VkSwapchainKHR newSwapchain;
    if (vkCreateSwapchainKHR(m_device, &info, nullptr, &newSwapchain) != VK_SUCCESS) return false;

    for (auto iv : m_swapchainImageViews) vkDestroyImageView(m_device, iv, nullptr);
    m_swapchainImageViews.clear();
    m_swapchainImages.clear();
    if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    m_swapchain = newSwapchain;
    m_swapchainFormat = format.format;
    m_swapchainExtent = extent;

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());

    m_swapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchainFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

VkFormat VulkanContext::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                            VkImageTiling tiling,
                                            VkFormatFeatureFlags features) const {
    for (auto format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
    }
    return VK_FORMAT_UNDEFINED;
}

void VulkanContext::submitAndWait(VkCommandBuffer cmdBuf) const {
    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &cmdBuf;

    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(m_device, &fenceInfo, nullptr, &fence);
    vkQueueSubmit(m_graphicsQueue, 1, &info, fence);
    vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(m_device, fence, nullptr);
}

VkResult VulkanContext::acquireNextImage(VkSemaphore semaphore, uint32_t* imageIndex) const {
    return vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                  semaphore, VK_NULL_HANDLE, imageIndex);
}

VkResult VulkanContext::present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) const {
    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &waitSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &m_swapchain;
    info.pImageIndices = &imageIndex;
    return vkQueuePresentKHR(queue, &info);
}

} // namespace Crab
