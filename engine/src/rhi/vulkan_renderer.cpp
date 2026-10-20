#include "crab/rhi/renderer.hpp"

#include "crab/core/log.hpp"
#include "crab/platform/window.hpp"
#include "crab/rhi/camera.hpp"

#include <volk.h>

#include <glm/gtc/matrix_transform.hpp>

#include <VkBootstrap.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl3.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
#include <vk_mem_alloc.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef CRAB_SHADER_DIR
#define CRAB_SHADER_DIR "shaders"
#endif

namespace crab::rhi {
namespace {

constexpr std::uint32_t kFramesInFlight = 2;
constexpr std::uint32_t kTextureSize = 64;

// Validation callbacks are free functions, so the counter is process-wide. A
// renderer resets it before creating its instance and exposes the count in
// RendererStats so tests can assert that debug runs stay validation-clean.
std::atomic<std::uint32_t> g_validationErrors{0};

// Matches the GLSL uniform block with std140 rules: `time` is at offset 0 and
// the `vec2 resolution` is 8-byte aligned at offset 8.
struct UniformData {
  float time = 0.0F;
  float padding = 0.0F;
  float width = 1.0F;
  float height = 1.0F;
  glm::mat4 viewProjection{1.0F};
  glm::mat4 model{1.0F};
};

static_assert(sizeof(UniformData) == 144);
static_assert(offsetof(UniformData, width) == 8);
static_assert(offsetof(UniformData, height) == 12);

struct MeshVertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

static_assert(sizeof(MeshVertex) == 32);

struct SpritePushConstants {
  glm::mat4 transform{1.0F};
  glm::vec4 tint{1.0F};
};

static_assert(sizeof(SpritePushConstants) == 80);

const char* vkResultName(VkResult result) noexcept {
  switch (result) {
  case VK_SUCCESS:
    return "VK_SUCCESS";
  case VK_NOT_READY:
    return "VK_NOT_READY";
  case VK_TIMEOUT:
    return "VK_TIMEOUT";
  case VK_EVENT_SET:
    return "VK_EVENT_SET";
  case VK_EVENT_RESET:
    return "VK_EVENT_RESET";
  case VK_INCOMPLETE:
    return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_FRAGMENTED_POOL:
    return "VK_ERROR_FRAGMENTED_POOL";
  case VK_ERROR_OUT_OF_POOL_MEMORY:
    return "VK_ERROR_OUT_OF_POOL_MEMORY";
  case VK_ERROR_SURFACE_LOST_KHR:
    return "VK_ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  case VK_SUBOPTIMAL_KHR:
    return "VK_SUBOPTIMAL_KHR";
  case VK_ERROR_OUT_OF_DATE_KHR:
    return "VK_ERROR_OUT_OF_DATE_KHR";
  default:
    return "VK_ERROR_UNKNOWN";
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
              const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) {
  (void)userData;
  (void)type;

  const char* message = callbackData != nullptr && callbackData->pMessage != nullptr
                          ? callbackData->pMessage
                          : "(validation message without text)";
  if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
    g_validationErrors.fetch_add(1, std::memory_order_relaxed);
    CRAB_LOG_ERROR("vulkan", "{}", message);
  } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U) {
    CRAB_LOG_WARN("vulkan", "{}", message);
  } else {
    CRAB_LOG_DEBUG("vulkan", "{}", message);
  }
  return VK_FALSE;
}

bool validationLayerAvailable() noexcept {
  std::uint32_t count = 0;
  if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS || count == 0) {
    return false;
  }

  std::vector<VkLayerProperties> layers(count);
  if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS) {
    return false;
  }

  return std::any_of(layers.begin(), layers.end(), [](const VkLayerProperties& layer) {
    return std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0;
  });
}

std::vector<std::uint32_t> readSpirv(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return {};
  }

  const std::streamoff end = file.tellg();
  if (end <= 0 || (end % 4) != 0) {
    return {};
  }

  std::vector<std::uint32_t> data(static_cast<std::size_t>(end) / sizeof(std::uint32_t));
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char*>(data.data()), end);
  if (!file) {
    return {};
  }
  return data;
}

bool hasShaderSet(const std::filesystem::path& directory) {
  constexpr std::array<const char*, 8> kShaderFiles = {
    "fullscreen.vert.spv", "fullscreen.frag.spv", "mesh.vert.spv",   "mesh.frag.spv",
    "grid.vert.spv",       "grid.frag.spv",       "sprite.vert.spv", "sprite.frag.spv",
  };

  std::error_code errorCode;
  return std::all_of(kShaderFiles.begin(), kShaderFiles.end(), [&](const char* file) {
    return std::filesystem::exists(directory / file, errorCode);
  });
}

std::filesystem::path findShaderDirectory() {
  std::vector<std::filesystem::path> candidates;
  candidates.emplace_back(CRAB_SHADER_DIR);

  // SDL3 owns and caches the base-path string; it must not be freed here.
  if (const char* basePath = SDL_GetBasePath()) {
    candidates.push_back(std::filesystem::path(basePath) / "shaders");
  }

  std::error_code errorCode;
  const std::filesystem::path workingDirectory = std::filesystem::current_path(errorCode);
  if (!errorCode) {
    candidates.push_back(workingDirectory / "shaders");
  }

  for (const std::filesystem::path& candidate : candidates) {
    if (hasShaderSet(candidate)) {
      return candidate;
    }
  }
  return candidates.front();
}

} // namespace

struct Renderer::Impl {
  struct Frame {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    VmaAllocation uniformAllocation = VK_NULL_HANDLE;
    void* uniformMapped = nullptr;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkImage depthImage = VK_NULL_HANDLE;
    VmaAllocation depthAllocation = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    std::uint32_t imageIndex = 0;
  };

  RendererConfig config;
  platform::Window* window = nullptr;
  bool headless = false;
  bool initialized = false;
  bool frameActive = false;
  bool frameAborted = false;
  bool imguiContextCreated = false;
  bool imguiInitialized = false;
  bool imguiSdlInitialized = false;
  bool imguiFrameActive = false;
  bool imguiFrameRendered = false;
  bool showOverlay = true;
  bool validationEnabled = false;
  std::array<bool, 6> cameraMovement{};
  bool cameraFast = false;
  bool resizeRequested = false;
  bool swapchainValid = false;

  std::string error;
  std::string deviceName;

  vkb::Instance instance;
  VkInstance instanceHandle = VK_NULL_HANDLE;
  vkb::PhysicalDevice physicalDevice;
  VkPhysicalDevice physicalDeviceHandle = VK_NULL_HANDLE;
  vkb::Device device;
  VkDevice deviceHandle = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;

  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue presentQueue = VK_NULL_HANDLE;
  std::uint32_t graphicsQueueFamily = 0;

  vkb::Swapchain swapchain;
  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;
  std::vector<VkImageLayout> swapchainLayouts;
  VkFormat colorFormat = VK_FORMAT_UNDEFINED;
  VkFormat depthFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D extent{0, 0};
  std::uint32_t swapchainImageCount = 0;

  VmaAllocator allocator = VK_NULL_HANDLE;
  VkCommandPool commandPool = VK_NULL_HANDLE;

  std::array<Frame, kFramesInFlight> frames{};
  std::uint32_t currentFrame = 0;
  std::uint32_t currentImageIndex = 0;

  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkPipeline meshPipeline = VK_NULL_HANDLE;
  VkPipeline gridPipeline = VK_NULL_HANDLE;
  VkPipeline spritePipeline = VK_NULL_HANDLE;

  VkBuffer cubeVertexBuffer = VK_NULL_HANDLE;
  VmaAllocation cubeVertexAllocation = VK_NULL_HANDLE;
  VkBuffer cubeIndexBuffer = VK_NULL_HANDLE;
  VmaAllocation cubeIndexAllocation = VK_NULL_HANDLE;
  std::uint32_t cubeIndexCount = 0;
  VkBuffer gridVertexBuffer = VK_NULL_HANDLE;
  VmaAllocation gridVertexAllocation = VK_NULL_HANDLE;
  std::uint32_t gridVertexCount = 0;

  PerspectiveCamera camera;

  VkImage textureImage = VK_NULL_HANDLE;
  VmaAllocation textureAllocation = VK_NULL_HANDLE;
  VkImageView textureView = VK_NULL_HANDLE;
  VkSampler textureSampler = VK_NULL_HANDLE;

  VkImage offscreenImage = VK_NULL_HANDLE;
  VmaAllocation offscreenAllocation = VK_NULL_HANDLE;
  VkImageView offscreenView = VK_NULL_HANDLE;
  VkImageLayout offscreenLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkBuffer readbackBuffer = VK_NULL_HANDLE;
  VmaAllocation readbackAllocation = VK_NULL_HANDLE;
  void* readbackMapped = nullptr;

  RendererStats stats;
  std::chrono::steady_clock::time_point startTime;
  std::chrono::steady_clock::time_point lastCameraUpdate;
  std::uint64_t renderedFrames = 0;

  [[nodiscard]] bool check(VkResult result, std::string_view operation) {
    if (result == VK_SUCCESS) {
      return true;
    }
    error = std::string(operation) + " failed: " + vkResultName(result);
    CRAB_LOG_ERROR("rhi", "{}", error);
    return false;
  }

  template <typename T>
  [[nodiscard]] bool checkVkb(const vkb::Result<T>& result, std::string_view operation) {
    if (result.has_value()) {
      return true;
    }
    error = std::string(operation) + " failed: " + result.error().message();
    CRAB_LOG_ERROR("rhi", "{}", error);
    return false;
  }

  template <typename Handle> static std::uint64_t handleValue(Handle handle) noexcept {
    if constexpr (std::is_pointer_v<Handle>) {
      return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
    } else {
      return static_cast<std::uint64_t>(handle);
    }
  }

  template <typename Handle>
  void setObjectName(VkObjectType objectType, Handle handle, const char* name) const {
    if (!validationEnabled || objectType == VK_OBJECT_TYPE_UNKNOWN || name == nullptr ||
        vkSetDebugUtilsObjectNameEXT == nullptr) {
      return;
    }

    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = handleValue(handle);
    nameInfo.pObjectName = name;
    vkSetDebugUtilsObjectNameEXT(deviceHandle, &nameInfo);
  }

  bool initializeCommon(const RendererConfig& rendererConfig, bool isHeadless) {
    config = rendererConfig;
    headless = isHeadless;
    validationEnabled = false;
    resizeRequested = false;
    swapchainValid = false;
    frameActive = false;
    frameAborted = false;
    error.clear();
    g_validationErrors.store(0, std::memory_order_relaxed);
    startTime = std::chrono::steady_clock::now();
    lastCameraUpdate = startTime;
    cameraMovement.fill(false);
    cameraFast = false;
    stats = {};
    stats.headless = headless;

    const VkResult loaderResult = volkInitialize();
    if (loaderResult != VK_SUCCESS) {
      error = "Vulkan loader is unavailable (volkInitialize failed)";
      CRAB_LOG_ERROR("rhi", "{}", error);
      return false;
    }

    if (!createInstance()) {
      return false;
    }
    if (!headless && !createSurface()) {
      return false;
    }
    if (!createDevice()) {
      return false;
    }
    if (!createAllocator()) {
      return false;
    }
    if (!createCommandPool()) {
      return false;
    }
    if (!headless && !createSwapchain()) {
      return false;
    }
    if (headless && !createOffscreenTarget()) {
      return false;
    }
    depthFormat = chooseDepthFormat();
    if (depthFormat == VK_FORMAT_UNDEFINED) {
      error = "No depth-attachment format is available";
      CRAB_LOG_ERROR("rhi", "{}", error);
      return false;
    }
    if (!createDescriptorSetLayout()) {
      return false;
    }
    if (!createPipelineLayout()) {
      return false;
    }
    if (!createFrames()) {
      return false;
    }
    if (!createDepthResources()) {
      return false;
    }
    if (!createDescriptorPool()) {
      return false;
    }
    if (!createTexture()) {
      return false;
    }
    if (!createDescriptorSets()) {
      return false;
    }
    if (!createMeshResources()) {
      return false;
    }
    if (!createPipelines()) {
      return false;
    }
    if (!createImgui()) {
      return false;
    }

    stats.deviceName = deviceName;
    stats.framesInFlight = kFramesInFlight;
    stats.validation = validationEnabled;
    initialized = true;
    updateStats();
    return true;
  }

  bool createInstance() {
    vkb::InstanceBuilder builder;
    builder.set_app_name(config.applicationName.c_str())
      .set_engine_name(config.engineName.c_str())
      .set_app_version(VK_MAKE_VERSION(0, 1, 0))
      .set_engine_version(VK_MAKE_VERSION(0, 1, 0))
      .require_api_version(VK_API_VERSION_1_3)
      .set_headless(headless);

    if (config.validation) {
      if (validationLayerAvailable()) {
        builder.request_validation_layers(true);
        builder.set_debug_callback(debugCallback);
        validationEnabled = true;
      } else {
        CRAB_LOG_WARN("rhi", "Vulkan validation layer not found; continuing without validation");
      }
    }

    if (!headless) {
      std::uint32_t extensionCount = 0;
      const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
      if (extensions == nullptr) {
        error = std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError();
        CRAB_LOG_ERROR("rhi", "{}", error);
        return false;
      }
      builder.enable_extensions(extensionCount, extensions);
    }

    auto instanceResult = builder.build();
    if (!checkVkb(instanceResult, "Creating Vulkan instance")) {
      return false;
    }

    instance = instanceResult.value();
    instanceHandle = instance.instance;
    volkLoadInstance(instanceHandle);

    CRAB_LOG_INFO("rhi", "Vulkan instance created (validation: {})",
                  validationEnabled ? "enabled" : "disabled");
    return true;
  }

  bool createSurface() {
    if (!SDL_Vulkan_CreateSurface(window->nativeHandle(), instanceHandle, nullptr, &surface)) {
      error = std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError();
      CRAB_LOG_ERROR("rhi", "{}", error);
      return false;
    }
    return true;
  }

  bool createDevice() {
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{instance};
    selector.set_minimum_version(1, 3)
      .add_required_extension_features(features13)
      .prefer_gpu_device_type(config.preferDiscreteGpu ? vkb::PreferredDeviceType::discrete
                                                       : vkb::PreferredDeviceType::integrated)
      .allow_any_gpu_device_type(true);
    if (headless) {
      selector.require_present(false);
    } else {
      selector.set_surface(surface);
    }

    auto physicalResult = selector.select();
    if (!checkVkb(physicalResult, "Selecting Vulkan physical device")) {
      return false;
    }
    physicalDevice = physicalResult.value();
    physicalDeviceHandle = physicalDevice;
    deviceName = physicalDevice.properties.deviceName;

    auto deviceResult = vkb::DeviceBuilder{physicalDevice}.build();
    if (!checkVkb(deviceResult, "Creating Vulkan logical device")) {
      return false;
    }
    device = deviceResult.value();
    deviceHandle = device.device;
    volkLoadDevice(deviceHandle);

    auto graphicsQueueResult = device.get_queue(vkb::QueueType::graphics);
    auto graphicsIndexResult = device.get_queue_index(vkb::QueueType::graphics);
    if (!checkVkb(graphicsQueueResult, "Getting graphics queue") ||
        !checkVkb(graphicsIndexResult, "Getting graphics queue family")) {
      return false;
    }
    graphicsQueue = graphicsQueueResult.value();
    graphicsQueueFamily = graphicsIndexResult.value();

    presentQueue = graphicsQueue;
    if (!headless) {
      auto presentQueueResult = device.get_queue(vkb::QueueType::present);
      auto presentIndexResult = device.get_queue_index(vkb::QueueType::present);
      if (presentQueueResult.has_value() && presentIndexResult.has_value()) {
        presentQueue = presentQueueResult.value();
      } else {
        CRAB_LOG_WARN("rhi", "Present queue unavailable; attempting to use the graphics queue");
      }
    }

    const VkPhysicalDeviceProperties& properties = physicalDevice.properties;
    CRAB_LOG_INFO("rhi", "Using Vulkan device: {} (API {}.{}.{})", deviceName,
                  VK_API_VERSION_MAJOR(properties.apiVersion),
                  VK_API_VERSION_MINOR(properties.apiVersion),
                  VK_API_VERSION_PATCH(properties.apiVersion));
    return true;
  }

  bool createAllocator() {
    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo createInfo{};
    createInfo.instance = instanceHandle;
    createInfo.physicalDevice = physicalDeviceHandle;
    createInfo.device = deviceHandle;
    createInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    createInfo.pVulkanFunctions = &functions;

    return check(vmaCreateAllocator(&createInfo, &allocator), "Creating VMA allocator");
  }

  bool createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;
    return check(vkCreateCommandPool(deviceHandle, &poolInfo, nullptr, &commandPool),
                 "Creating command pool");
  }

  bool createSwapchain() {
    const int pixelWidth = window->width();
    const int pixelHeight = window->height();
    if (pixelWidth <= 0 || pixelHeight <= 0) {
      resizeRequested = true;
      return false;
    }

    vkb::SwapchainBuilder builder{device, surface};
    builder.set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
      .add_fallback_format({VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
      .set_desired_min_image_count(2)
      .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
      .set_desired_extent(static_cast<std::uint32_t>(pixelWidth),
                          static_cast<std::uint32_t>(pixelHeight));

    auto swapchainResult = builder.build();
    if (!checkVkb(swapchainResult, "Creating swapchain")) {
      return false;
    }

    vkb::Swapchain newSwapchain = swapchainResult.value();
    auto imagesResult = newSwapchain.get_images_and_image_views();
    if (!checkVkb(imagesResult, "Getting swapchain images")) {
      vkb::destroy_swapchain(newSwapchain);
      return false;
    }

    auto [newImages, newViews] = imagesResult.value();
    swapchain = newSwapchain;
    swapchainImages = std::move(newImages);
    swapchainImageViews = std::move(newViews);
    swapchainImageCount = static_cast<std::uint32_t>(swapchainImages.size());
    swapchainLayouts.assign(swapchainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    colorFormat = swapchain.image_format;
    extent = swapchain.extent;
    swapchainValid = true;
    resizeRequested = false;

    for (std::size_t index = 0; index < swapchainImages.size(); ++index) {
      const std::string name = "SwapchainImage" + std::to_string(index);
      setObjectName(VK_OBJECT_TYPE_IMAGE, swapchainImages[index], name.c_str());
    }
    setObjectName(VK_OBJECT_TYPE_SWAPCHAIN_KHR, swapchain.swapchain, "MainSwapchain");

    CRAB_LOG_INFO("rhi", "Swapchain: {}x{}, {} images, format {}, present mode {}", extent.width,
                  extent.height, swapchainImageCount, static_cast<int>(colorFormat),
                  static_cast<int>(swapchain.present_mode));
    updateStats();
    return true;
  }

  bool createOffscreenTarget() {
    colorFormat = chooseOffscreenFormat();
    if (colorFormat == VK_FORMAT_UNDEFINED) {
      error = "No color-attachment format is available for headless rendering";
      CRAB_LOG_ERROR("rhi", "{}", error);
      return false;
    }

    if (!createImage(extent.width, extent.height, colorFormat,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     offscreenImage, offscreenAllocation, offscreenView)) {
      return false;
    }

    const VkDeviceSize readbackSize =
      static_cast<VkDeviceSize>(extent.width) * static_cast<VkDeviceSize>(extent.height) * 4U;
    if (!createBuffer(readbackSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
                      readbackBuffer, readbackAllocation, &readbackMapped)) {
      return false;
    }

    setObjectName(VK_OBJECT_TYPE_IMAGE, offscreenImage, "HeadlessColorImage");
    setObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, offscreenView, "HeadlessColorView");
    setObjectName(VK_OBJECT_TYPE_BUFFER, readbackBuffer, "HeadlessReadbackBuffer");

    offscreenLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainImageCount = 1;
    updateStats();
    CRAB_LOG_INFO("rhi", "Headless target: {}x{}, format {}", extent.width, extent.height,
                  static_cast<int>(colorFormat));
    return true;
  }

  VkFormat chooseOffscreenFormat() const {
    const VkFormat candidates[] = {
      VK_FORMAT_B8G8R8A8_SRGB,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_R8G8B8A8_UNORM,
    };

    for (const VkFormat candidate : candidates) {
      VkFormatProperties properties{};
      vkGetPhysicalDeviceFormatProperties(physicalDeviceHandle, candidate, &properties);
      if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0U) {
        return candidate;
      }
    }
    return VK_FORMAT_UNDEFINED;
  }

  VkFormat chooseDepthFormat() const {
    // Depth-only formats keep the rendering setup simple and portable. D32 is
    // preferred for precision; D16 is a widely available fallback.
    const VkFormat candidates[] = {
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D16_UNORM,
    };

    for (const VkFormat candidate : candidates) {
      VkFormatProperties properties{};
      vkGetPhysicalDeviceFormatProperties(physicalDeviceHandle, candidate, &properties);
      if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) !=
          0U) {
        return candidate;
      }
    }
    return VK_FORMAT_UNDEFINED;
  }

  bool createDepthResources() {
    destroyDepthResources();
    for (std::uint32_t index = 0; index < kFramesInFlight; ++index) {
      Frame& frame = frames[index];
      if (!createImage(extent.width, extent.height, depthFormat,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, frame.depthImage,
                       frame.depthAllocation, frame.depthView, VK_IMAGE_ASPECT_DEPTH_BIT)) {
        destroyDepthResources();
        return false;
      }
      const std::string imageName = "DepthImage" + std::to_string(index);
      const std::string viewName = "DepthImageView" + std::to_string(index);
      setObjectName(VK_OBJECT_TYPE_IMAGE, frame.depthImage, imageName.c_str());
      setObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, frame.depthView, viewName.c_str());
    }
    return true;
  }

  void destroyDepthResources() {
    for (Frame& frame : frames) {
      if (frame.depthView != VK_NULL_HANDLE) {
        vkDestroyImageView(deviceHandle, frame.depthView, nullptr);
        frame.depthView = VK_NULL_HANDLE;
      }
      if (frame.depthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, frame.depthImage, frame.depthAllocation);
        frame.depthImage = VK_NULL_HANDLE;
        frame.depthAllocation = VK_NULL_HANDLE;
      }
    }
  }

  bool createImage(std::uint32_t width, std::uint32_t height, VkFormat format,
                   VkImageUsageFlags usage, VkImage& image, VmaAllocation& allocation,
                   VkImageView& view, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (!check(vmaCreateImage(allocator, &imageInfo, &allocationInfo, &image, &allocation, nullptr),
               "Creating Vulkan image")) {
      return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (!check(vkCreateImageView(deviceHandle, &viewInfo, nullptr, &view), "Creating image view")) {
      vmaDestroyImage(allocator, image, allocation);
      image = VK_NULL_HANDLE;
      allocation = VK_NULL_HANDLE;
      return false;
    }
    return true;
  }

  bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
                    VkBuffer& buffer, VmaAllocation& allocation, void** mapped = nullptr) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = memoryUsage;

    VmaAllocationInfo resultInfo{};
    if (!check(vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &buffer, &allocation,
                               &resultInfo),
               "Creating Vulkan buffer")) {
      return false;
    }

    if (mapped != nullptr) {
      if (!check(vmaMapMemory(allocator, allocation, mapped), "Mapping Vulkan buffer")) {
        vmaDestroyBuffer(allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        return false;
      }
    }
    return true;
  }

  bool beginOneShot(VkCommandBuffer& commandBuffer) {
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    if (!check(vkAllocateCommandBuffers(deviceHandle, &allocateInfo, &commandBuffer),
               "Allocating one-shot command buffer")) {
      return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!check(vkBeginCommandBuffer(commandBuffer, &beginInfo),
               "Beginning one-shot command buffer")) {
      vkFreeCommandBuffers(deviceHandle, commandPool, 1, &commandBuffer);
      commandBuffer = VK_NULL_HANDLE;
      return false;
    }
    return true;
  }

  bool endOneShot(VkCommandBuffer commandBuffer) {
    if (!check(vkEndCommandBuffer(commandBuffer), "Ending one-shot command buffer")) {
      vkFreeCommandBuffers(deviceHandle, commandPool, 1, &commandBuffer);
      return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    if (!check(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE),
               "Submitting one-shot command buffer")) {
      vkFreeCommandBuffers(deviceHandle, commandPool, 1, &commandBuffer);
      return false;
    }
    if (!check(vkQueueWaitIdle(graphicsQueue), "Waiting for one-shot command buffer")) {
      vkFreeCommandBuffers(deviceHandle, commandPool, 1, &commandBuffer);
      return false;
    }

    vkFreeCommandBuffers(deviceHandle, commandPool, 1, &commandBuffer);
    return true;
  }

  void imageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
                    VkImageLayout newLayout, VkAccessFlags2 sourceAccess,
                    VkAccessFlags2 destinationAccess, VkPipelineStageFlags2 sourceStage,
                    VkPipelineStageFlags2 destinationStage,
                    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) const {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = sourceStage;
    barrier.srcAccessMask = sourceAccess;
    barrier.dstStageMask = destinationStage;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
  }

  void bufferBarrier(VkCommandBuffer commandBuffer, VkBuffer buffer, VkAccessFlags2 sourceAccess,
                     VkAccessFlags2 destinationAccess, VkPipelineStageFlags2 sourceStage,
                     VkPipelineStageFlags2 destinationStage) const {
    VkBufferMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.srcStageMask = sourceStage;
    barrier.srcAccessMask = sourceAccess;
    barrier.dstStageMask = destinationStage;
    barrier.dstAccessMask = destinationAccess;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = 1;
    dependencyInfo.pBufferMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
  }

  bool uploadBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer,
                    VmaAllocation& allocation, const char* name) {
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    void* stagingMapped = nullptr;
    if (!createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
                      stagingBuffer, stagingAllocation, &stagingMapped)) {
      return false;
    }

    std::memcpy(stagingMapped, data, static_cast<std::size_t>(size));
    vmaFlushAllocation(allocator, stagingAllocation, 0, VK_WHOLE_SIZE);
    vmaUnmapMemory(allocator, stagingAllocation);

    if (!createBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                      buffer, allocation)) {
      vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
      return false;
    }

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (!beginOneShot(commandBuffer)) {
      vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
      vmaDestroyBuffer(allocator, buffer, allocation);
      buffer = VK_NULL_HANDLE;
      allocation = VK_NULL_HANDLE;
      return false;
    }

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);
    bufferBarrier(commandBuffer, buffer, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT,
                  VK_PIPELINE_STAGE_2_COPY_BIT, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT);

    const bool submitted = endOneShot(commandBuffer);
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
    if (!submitted) {
      vmaDestroyBuffer(allocator, buffer, allocation);
      buffer = VK_NULL_HANDLE;
      allocation = VK_NULL_HANDLE;
      return false;
    }

    setObjectName(VK_OBJECT_TYPE_BUFFER, buffer, name);
    return true;
  }

  bool createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    if (!check(
          vkCreateDescriptorSetLayout(deviceHandle, &layoutInfo, nullptr, &descriptorSetLayout),
          "Creating descriptor set layout")) {
      return false;
    }
    setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, descriptorSetLayout,
                  "FullscreenDescriptorSetLayout");
    return true;
  }

  bool createPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SpritePushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (!check(vkCreatePipelineLayout(deviceHandle, &layoutInfo, nullptr, &pipelineLayout),
               "Creating pipeline layout")) {
      return false;
    }
    setObjectName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, pipelineLayout, "FullscreenPipelineLayout");
    return true;
  }

  bool createFrames() {
    for (std::uint32_t index = 0; index < kFramesInFlight; ++index) {
      Frame& frame = frames[index];

      VkCommandBufferAllocateInfo commandInfo{};
      commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      commandInfo.commandPool = commandPool;
      commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      commandInfo.commandBufferCount = 1;
      if (!check(vkAllocateCommandBuffers(deviceHandle, &commandInfo, &frame.commandBuffer),
                 "Allocating frame command buffer")) {
        return false;
      }
      const std::string commandBufferName = "FrameCommandBuffer" + std::to_string(index);
      setObjectName(VK_OBJECT_TYPE_COMMAND_BUFFER, frame.commandBuffer, commandBufferName.c_str());

      VkFenceCreateInfo fenceInfo{};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      if (!check(vkCreateFence(deviceHandle, &fenceInfo, nullptr, &frame.fence),
                 "Creating frame fence")) {
        return false;
      }

      if (!headless) {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (!check(vkCreateSemaphore(deviceHandle, &semaphoreInfo, nullptr, &frame.imageAvailable),
                   "Creating image-available semaphore") ||
            !check(vkCreateSemaphore(deviceHandle, &semaphoreInfo, nullptr, &frame.renderFinished),
                   "Creating render-finished semaphore")) {
          return false;
        }
      }

      if (!createBuffer(sizeof(UniformData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VMA_MEMORY_USAGE_CPU_TO_GPU, frame.uniformBuffer, frame.uniformAllocation,
                        &frame.uniformMapped)) {
        return false;
      }
      const std::string uniformBufferName = "UniformBuffer" + std::to_string(index);
      setObjectName(VK_OBJECT_TYPE_BUFFER, frame.uniformBuffer, uniformBufferName.c_str());
    }
    return true;
  }

  bool createDescriptorPool() {
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = kFramesInFlight;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = kFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = kFramesInFlight;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;

    return check(vkCreateDescriptorPool(deviceHandle, &poolInfo, nullptr, &descriptorPool),
                 "Creating descriptor pool");
  }

  bool createTexture() {
    std::vector<std::uint8_t> pixels(kTextureSize * kTextureSize * 4);
    for (std::uint32_t y = 0; y < kTextureSize; ++y) {
      for (std::uint32_t x = 0; x < kTextureSize; ++x) {
        const std::uint32_t index = (y * kTextureSize + x) * 4;
        const bool light = ((x / 8) + (y / 8)) % 2 == 0;
        const std::uint8_t base = light ? 210 : 48;
        const std::uint8_t tint = static_cast<std::uint8_t>(std::min(255U, base + (x * 2U)));
        pixels[index + 0] = tint;
        pixels[index + 1] = base;
        pixels[index + 2] = static_cast<std::uint8_t>(255U - base / 2U);
        pixels[index + 3] = 255;
      }
    }

    const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    if (!createImage(kTextureSize, kTextureSize, format,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, textureImage,
                     textureAllocation, textureView)) {
      return false;
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    void* stagingMapped = nullptr;
    if (!createBuffer(static_cast<VkDeviceSize>(pixels.size()), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation,
                      &stagingMapped)) {
      return false;
    }
    std::memcpy(stagingMapped, pixels.data(), pixels.size());
    vmaFlushAllocation(allocator, stagingAllocation, 0, VK_WHOLE_SIZE);
    vmaUnmapMemory(allocator, stagingAllocation);

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (!beginOneShot(commandBuffer)) {
      vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
      return false;
    }

    imageBarrier(commandBuffer, textureImage, VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COPY_BIT);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {kTextureSize, kTextureSize, 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, textureImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    imageBarrier(commandBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                 VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT,
                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    if (!endOneShot(commandBuffer)) {
      vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
      return false;
    }
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.minLod = 0.0F;
    samplerInfo.maxLod = 0.0F;
    samplerInfo.maxAnisotropy = 1.0F;
    if (!check(vkCreateSampler(deviceHandle, &samplerInfo, nullptr, &textureSampler),
               "Creating texture sampler")) {
      return false;
    }

    setObjectName(VK_OBJECT_TYPE_IMAGE, textureImage, "CheckerTexture");
    setObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, textureView, "CheckerTextureView");
    setObjectName(VK_OBJECT_TYPE_SAMPLER, textureSampler, "CheckerSampler");
    return true;
  }

  bool createDescriptorSets() {
    std::array<VkDescriptorSetLayout, kFramesInFlight> layouts{};
    layouts.fill(descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = descriptorPool;
    allocateInfo.descriptorSetCount = kFramesInFlight;
    allocateInfo.pSetLayouts = layouts.data();

    std::array<VkDescriptorSet, kFramesInFlight> descriptorSets{};
    if (!check(vkAllocateDescriptorSets(deviceHandle, &allocateInfo, descriptorSets.data()),
               "Allocating descriptor sets")) {
      return false;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = textureSampler;
    imageInfo.imageView = textureView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    for (std::uint32_t index = 0; index < kFramesInFlight; ++index) {
      Frame& frame = frames[index];
      frame.descriptorSet = descriptorSets[index];

      VkDescriptorBufferInfo bufferInfo{};
      bufferInfo.buffer = frame.uniformBuffer;
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(UniformData);

      VkWriteDescriptorSet writes[2]{};
      writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[0].dstSet = frame.descriptorSet;
      writes[0].dstBinding = 0;
      writes[0].descriptorCount = 1;
      writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writes[0].pImageInfo = &imageInfo;

      writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[1].dstSet = frame.descriptorSet;
      writes[1].dstBinding = 1;
      writes[1].descriptorCount = 1;
      writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      writes[1].pBufferInfo = &bufferInfo;

      vkUpdateDescriptorSets(deviceHandle, 2, writes, 0, nullptr);
    }
    return true;
  }

  bool createMeshResources() {
    std::vector<MeshVertex> vertices;
    vertices.reserve(24);
    std::vector<std::uint16_t> indices;
    indices.reserve(36);

    const auto addFace = [&](const glm::vec3& normal, const glm::vec3& a, const glm::vec3& b,
                             const glm::vec3& c, const glm::vec3& d) {
      const auto base = static_cast<std::uint16_t>(vertices.size());
      vertices.push_back({a, normal, {0.0F, 1.0F}});
      vertices.push_back({b, normal, {1.0F, 1.0F}});
      vertices.push_back({c, normal, {1.0F, 0.0F}});
      vertices.push_back({d, normal, {0.0F, 0.0F}});
      indices.insert(indices.end(), {base, static_cast<std::uint16_t>(base + 1U),
                                     static_cast<std::uint16_t>(base + 2U), base,
                                     static_cast<std::uint16_t>(base + 2U),
                                     static_cast<std::uint16_t>(base + 3U)});
    };

    addFace({0.0F, 0.0F, 1.0F}, {-0.5F, -0.5F, 0.5F}, {0.5F, -0.5F, 0.5F}, {0.5F, 0.5F, 0.5F},
            {-0.5F, 0.5F, 0.5F});
    addFace({0.0F, 0.0F, -1.0F}, {0.5F, -0.5F, -0.5F}, {-0.5F, -0.5F, -0.5F}, {-0.5F, 0.5F, -0.5F},
            {0.5F, 0.5F, -0.5F});
    addFace({-1.0F, 0.0F, 0.0F}, {-0.5F, -0.5F, -0.5F}, {-0.5F, -0.5F, 0.5F}, {-0.5F, 0.5F, 0.5F},
            {-0.5F, 0.5F, -0.5F});
    addFace({1.0F, 0.0F, 0.0F}, {0.5F, -0.5F, 0.5F}, {0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, -0.5F},
            {0.5F, 0.5F, 0.5F});
    addFace({0.0F, 1.0F, 0.0F}, {-0.5F, 0.5F, 0.5F}, {0.5F, 0.5F, 0.5F}, {0.5F, 0.5F, -0.5F},
            {-0.5F, 0.5F, -0.5F});
    addFace({0.0F, -1.0F, 0.0F}, {-0.5F, -0.5F, -0.5F}, {0.5F, -0.5F, -0.5F}, {0.5F, -0.5F, 0.5F},
            {-0.5F, -0.5F, 0.5F});

    if (!uploadBuffer(vertices.data(), sizeof(MeshVertex) * vertices.size(),
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, cubeVertexBuffer, cubeVertexAllocation,
                      "CubeVertexBuffer") ||
        !uploadBuffer(indices.data(), sizeof(std::uint16_t) * indices.size(),
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT, cubeIndexBuffer, cubeIndexAllocation,
                      "CubeIndexBuffer")) {
      return false;
    }
    cubeIndexCount = static_cast<std::uint32_t>(indices.size());

    std::vector<glm::vec3> gridVertices;
    gridVertices.reserve(44);
    for (int coordinate = -5; coordinate <= 5; ++coordinate) {
      gridVertices.emplace_back(static_cast<float>(coordinate), -1.0F, -5.0F);
      gridVertices.emplace_back(static_cast<float>(coordinate), -1.0F, 5.0F);
      gridVertices.emplace_back(-5.0F, -1.0F, static_cast<float>(coordinate));
      gridVertices.emplace_back(5.0F, -1.0F, static_cast<float>(coordinate));
    }

    if (!uploadBuffer(gridVertices.data(), sizeof(glm::vec3) * gridVertices.size(),
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, gridVertexBuffer, gridVertexAllocation,
                      "GridVertexBuffer")) {
      return false;
    }
    gridVertexCount = static_cast<std::uint32_t>(gridVertices.size());
    CRAB_LOG_DEBUG("rhi", "Uploaded demo mesh: {} vertices, {} indices, {} grid vertices",
                   vertices.size(), indices.size(), gridVertices.size());
    return true;
  }

  bool
  createGraphicsPipeline(std::string_view vertexShaderFile, std::string_view fragmentShaderFile,
                         VkPrimitiveTopology topology,
                         const std::vector<VkVertexInputBindingDescription>& vertexBindings,
                         const std::vector<VkVertexInputAttributeDescription>& vertexAttributes,
                         bool depthTest, bool depthWrite, bool enableBlend, VkPipeline& outPipeline,
                         const char* pipelineName) {
    const std::filesystem::path shaderDirectory = findShaderDirectory();
    const auto vertexCode = readSpirv(shaderDirectory / std::string(vertexShaderFile));
    const auto fragmentCode = readSpirv(shaderDirectory / std::string(fragmentShaderFile));
    if (vertexCode.empty() || fragmentCode.empty()) {
      error = "Failed to read compiled shaders from " + shaderDirectory.string() +
              " (run the crab_shaders target or set CRAB_SHADER_DIR)";
      CRAB_LOG_ERROR("rhi", "{}", error);
      return false;
    }

    VkShaderModule vertexModule = VK_NULL_HANDLE;
    VkShaderModule fragmentModule = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = vertexCode.size() * sizeof(std::uint32_t);
    moduleInfo.pCode = vertexCode.data();
    if (!check(vkCreateShaderModule(deviceHandle, &moduleInfo, nullptr, &vertexModule),
               "Creating vertex shader module")) {
      return false;
    }

    moduleInfo.codeSize = fragmentCode.size() * sizeof(std::uint32_t);
    moduleInfo.pCode = fragmentCode.data();
    if (!check(vkCreateShaderModule(deviceHandle, &moduleInfo, nullptr, &fragmentModule),
               "Creating fragment shader module")) {
      vkDestroyShaderModule(deviceHandle, vertexModule, nullptr);
      return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertexModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragmentModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertexBindings.size());
    vertexInput.pVertexBindingDescriptions = vertexBindings.data();
    vertexInput.vertexAttributeDescriptionCount =
      static_cast<std::uint32_t>(vertexAttributes.size());
    vertexInput.pVertexAttributeDescriptions = vertexAttributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{
      0.0F, 0.0F, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0F, 1.0F};
    VkRect2D scissor{{0, 0}, extent};
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable = VK_FALSE;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable = VK_FALSE;
    rasterization.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = depthTest ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = enableBlend ? VK_TRUE : VK_FALSE;
    if (enableBlend) {
      colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
      colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;

    if (outPipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(deviceHandle, outPipeline, nullptr);
      outPipeline = VK_NULL_HANDLE;
    }

    const VkResult result = vkCreateGraphicsPipelines(deviceHandle, VK_NULL_HANDLE, 1,
                                                      &pipelineInfo, nullptr, &outPipeline);
    vkDestroyShaderModule(deviceHandle, vertexModule, nullptr);
    vkDestroyShaderModule(deviceHandle, fragmentModule, nullptr);
    if (!check(result, "Creating graphics pipeline")) {
      return false;
    }
    setObjectName(VK_OBJECT_TYPE_PIPELINE, outPipeline, pipelineName);
    return true;
  }

  bool createPipelines() {
    const std::vector<VkVertexInputBindingDescription> meshBinding = {
      {0, sizeof(MeshVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    const std::vector<VkVertexInputAttributeDescription> meshAttributes = {
      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position)},
      {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, normal)},
      {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(MeshVertex, uv)},
    };
    const std::vector<VkVertexInputBindingDescription> gridBinding = {
      {0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    const std::vector<VkVertexInputAttributeDescription> gridAttributes = {
      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
    };

    return createGraphicsPipeline("fullscreen.vert.spv", "fullscreen.frag.spv",
                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, {}, {}, false, false, false,
                                  pipeline, "FullscreenPipeline") &&
           createGraphicsPipeline("mesh.vert.spv", "mesh.frag.spv",
                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, meshBinding, meshAttributes,
                                  true, true, false, meshPipeline, "MeshPipeline") &&
           createGraphicsPipeline("grid.vert.spv", "grid.frag.spv", VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                                  gridBinding, gridAttributes, true, false, false, gridPipeline,
                                  "GridPipeline") &&
           createGraphicsPipeline("sprite.vert.spv", "sprite.frag.spv",
                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, {}, {}, false, false, true,
                                  spritePipeline, "SpritePipeline");
  }

  bool initImguiVulkan() {
    const std::uint32_t imguiImageCount = headless ? 2U : std::max(2U, swapchain.image_count);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = instanceHandle;
    initInfo.PhysicalDevice = physicalDeviceHandle;
    initInfo.Device = deviceHandle;
    initInfo.QueueFamily = graphicsQueueFamily;
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPoolSize = 64;
    initInfo.MinImageCount = imguiImageCount;
    initInfo.ImageCount = imguiImageCount;
    initInfo.MinAllocationSize = 1024 * 1024;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
      error = "ImGui_ImplVulkan_Init failed";
      CRAB_LOG_ERROR("rhi", "{}", error);
      return false;
    }
    return true;
  }

  void destroyImgui() {
    if (imguiInitialized) {
      ImGui_ImplVulkan_Shutdown();
      imguiInitialized = false;
    }
    if (imguiSdlInitialized) {
      ImGui_ImplSDL3_Shutdown();
      imguiSdlInitialized = false;
    }
    if (imguiContextCreated) {
      ImGui::DestroyContext();
      imguiContextCreated = false;
    }
    imguiFrameActive = false;
    imguiFrameRendered = false;
  }

  bool createImgui() {
    if (!config.enableImGui) {
      return true;
    }

    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr) {
      error = "ImGui::CreateContext failed";
      CRAB_LOG_ERROR("rhi", "{}", error);
      return false;
    }
    imguiContextCreated = true;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    if (!headless) {
      if (!ImGui_ImplSDL3_InitForVulkan(window->nativeHandle())) {
        error = "ImGui_ImplSDL3_InitForVulkan failed";
        CRAB_LOG_ERROR("rhi", "{}", error);
        destroyImgui();
        return false;
      }
      imguiSdlInitialized = true;
    }

    if (!initImguiVulkan()) {
      destroyImgui();
      return false;
    }

    imguiInitialized = true;
    stats.imgui = true;
    return true;
  }

  bool recreateSwapchain() {
    if (headless) {
      return true;
    }
    if (window->width() <= 0 || window->height() <= 0) {
      return false;
    }

    if (!check(vkDeviceWaitIdle(deviceHandle), "Waiting for device before resize")) {
      frameAborted = true;
      return false;
    }

    const VkFormat previousFormat = colorFormat;
    const std::uint32_t previousImageCount = swapchain.image_count;
    const bool hadPipeline = pipeline != VK_NULL_HANDLE;

    if (swapchainValid) {
      swapchain.destroy_image_views(swapchainImageViews);
      swapchainImageViews.clear();
      vkb::destroy_swapchain(swapchain);
      swapchainValid = false;
      swapchainImages.clear();
      swapchainImageCount = 0;
    }

    if (!createSwapchain()) {
      return false;
    }
    if (!createDepthResources()) {
      frameAborted = true;
      return false;
    }
    if (hadPipeline && previousFormat != colorFormat) {
      if (!createPipelines()) {
        frameAborted = true;
        return false;
      }
    }
    if (imguiInitialized &&
        (previousFormat != colorFormat || previousImageCount != swapchain.image_count)) {
      ImGui_ImplVulkan_Shutdown();
      imguiInitialized = false;
      if (!initImguiVulkan()) {
        frameAborted = true;
        return false;
      }
      imguiInitialized = true;
      updateStats();
    }
    return true;
  }

  void updateStats() {
    stats.swapchainWidth = extent.width;
    stats.swapchainHeight = extent.height;
    stats.swapchainImageCount = swapchainImageCount;
    stats.framesInFlight = initialized ? kFramesInFlight : 0;
    stats.headless = headless;
    stats.imgui = imguiInitialized;
    stats.validation = validationEnabled;
    stats.validationErrors = g_validationErrors.load(std::memory_order_relaxed);
  }

  void updateCamera() {
    const auto now = std::chrono::steady_clock::now();
    const float deltaSeconds =
      std::min(std::chrono::duration<float>(now - lastCameraUpdate).count(), 0.1F);
    lastCameraUpdate = now;

    glm::vec3 movement(0.0F);
    if (cameraMovement[0]) {
      movement.z += 1.0F;
    }
    if (cameraMovement[1]) {
      movement.z -= 1.0F;
    }
    if (cameraMovement[2]) {
      movement.x -= 1.0F;
    }
    if (cameraMovement[3]) {
      movement.x += 1.0F;
    }
    if (cameraMovement[4]) {
      movement.y += 1.0F;
    }
    if (cameraMovement[5]) {
      movement.y -= 1.0F;
    }
    if (movement.x == 0.0F && movement.y == 0.0F && movement.z == 0.0F) {
      return;
    }

    const glm::vec3 forward = glm::normalize(camera.target - camera.position);
    const glm::vec3 right = glm::normalize(glm::cross(forward, camera.up));
    const glm::vec3 offset = (forward * movement.z + right * movement.x + camera.up * movement.y) *
                             (cameraFast ? 8.0F : 3.0F) * deltaSeconds;
    camera.position += offset;
    camera.target += offset;
  }

  void updateUniformBuffer(Frame& frame) {
    if (frame.uniformMapped == nullptr) {
      return;
    }

    updateCamera();
    const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
    auto* uniform = static_cast<UniformData*>(frame.uniformMapped);
    uniform->time = static_cast<float>(seconds);
    uniform->width = static_cast<float>(extent.width);
    uniform->height = static_cast<float>(extent.height);

    const float aspect = extent.height > 0
                           ? static_cast<float>(extent.width) / static_cast<float>(extent.height)
                           : 1.0F;
    uniform->viewProjection = camera.viewProjection(aspect);
    uniform->model = glm::rotate(glm::mat4(1.0F), static_cast<float>(seconds) * 0.45F,
                                 glm::vec3(0.0F, 1.0F, 0.0F));
    vmaFlushAllocation(allocator, frame.uniformAllocation, 0, VK_WHOLE_SIZE);
  }

  bool beginHeadlessFrame() {
    Frame& frame = frames[currentFrame];
    if (!check(vkWaitForFences(deviceHandle, 1, &frame.fence, VK_TRUE,
                               std::numeric_limits<std::uint64_t>::max()),
               "Waiting for frame fence")) {
      frameAborted = true;
      return false;
    }
    if (!check(vkResetFences(deviceHandle, 1, &frame.fence), "Resetting frame fence")) {
      frameAborted = true;
      return false;
    }

    frame.imageIndex = 0;
    currentImageIndex = 0;
    return true;
  }

  bool beginWindowFrame() {
    Frame& frame = frames[currentFrame];
    if (!check(vkWaitForFences(deviceHandle, 1, &frame.fence, VK_TRUE,
                               std::numeric_limits<std::uint64_t>::max()),
               "Waiting for frame fence")) {
      frameAborted = true;
      return false;
    }

    std::uint32_t imageIndex = 0;
    // A finite timeout keeps a minimized window from blocking the main loop
    // forever on drivers that do not immediately report an out-of-date
    // swapchain.
    const VkResult acquireResult =
      vkAcquireNextImageKHR(deviceHandle, swapchain.swapchain, 1'000'000'000ULL,
                            frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_TIMEOUT ||
        acquireResult == VK_NOT_READY) {
      resizeRequested = true;
      return false;
    }
    if (acquireResult == VK_SUBOPTIMAL_KHR) {
      resizeRequested = true;
    } else if (!check(acquireResult, "Acquiring swapchain image")) {
      frameAborted = true;
      return false;
    }

    if (!check(vkResetFences(deviceHandle, 1, &frame.fence), "Resetting frame fence")) {
      frameAborted = true;
      return false;
    }

    frame.imageIndex = imageIndex;
    currentImageIndex = imageIndex;
    return true;
  }

  bool recordFrame() {
    Frame& frame = frames[currentFrame];
    VkCommandBuffer commandBuffer = frame.commandBuffer;
    updateUniformBuffer(frame);

    if (!check(vkResetCommandBuffer(commandBuffer, 0), "Resetting frame command buffer")) {
      return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!check(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Beginning frame command buffer")) {
      return false;
    }

    const bool windowFrame = !headless;
    VkImage targetImage = windowFrame ? swapchainImages[currentImageIndex] : offscreenImage;
    const VkImageLayout oldLayout =
      windowFrame ? swapchainLayouts[currentImageIndex] : offscreenLayout;

    const VkPipelineStageFlags2 sourceStage = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
                                                ? VK_PIPELINE_STAGE_2_NONE
                                                : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    imageBarrier(commandBuffer, targetImage, oldLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, sourceStage,
                 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    imageBarrier(commandBuffer, frame.depthImage, VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0,
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE,
                 VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                 VK_IMAGE_ASPECT_DEPTH_BIT);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView =
      windowFrame ? swapchainImageViews[currentImageIndex] : offscreenView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color.float32[0] = 0.035F;
    colorAttachment.clearValue.color.float32[1] = 0.055F;
    colorAttachment.clearValue.color.float32[2] = 0.085F;
    colorAttachment.clearValue.color.float32[3] = 1.0F;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = frame.depthView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil.depth = 1.0F;
    depthAttachment.clearValue.depthStencil.stencil = 0;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;
    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    VkViewport viewport{
      0.0F, 0.0F, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0F, 1.0F};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
                            &frame.descriptorSet, 0, nullptr);
    const float brightness = 1.0F;
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(brightness), &brightness);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    const VkDeviceSize vertexOffset = 0;
    const glm::vec4 meshTint(1.0F);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(meshTint), &meshTint);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &cubeVertexBuffer, &vertexOffset);
    vkCmdBindIndexBuffer(commandBuffer, cubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(commandBuffer, cubeIndexCount, 1, 0, 0, 0);

    const glm::vec4 gridTint(0.34F, 0.40F, 0.52F, 1.0F);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline);
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(gridTint), &gridTint);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &gridVertexBuffer, &vertexOffset);
    vkCmdDraw(commandBuffer, gridVertexCount, 1, 0, 0);

    const float aspect = extent.height > 0
                           ? static_cast<float>(extent.width) / static_cast<float>(extent.height)
                           : 1.0F;
    OrthographicCamera orthoCamera;
    orthoCamera.width = aspect * 2.0F;
    orthoCamera.height = 2.0F;

    SpritePushConstants spritePush;
    spritePush.transform =
      orthoCamera.viewProjection() *
      glm::translate(glm::mat4(1.0F), glm::vec3(-orthoCamera.width * 0.38F, -0.72F, 0.0F)) *
      glm::scale(glm::mat4(1.0F), glm::vec3(0.9F, 0.9F, 1.0F));
    spritePush.tint = glm::vec4(1.0F, 1.0F, 1.0F, 0.92F);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spritePipeline);
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(spritePush), &spritePush);
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);

    if (imguiFrameActive) {
      ImGui::Render();
      imguiFrameRendered = true;
      ImDrawData* drawData = ImGui::GetDrawData();
      ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);

      std::uint32_t imguiDrawCalls = 0;
      std::uint32_t imguiVertices = 0;
      if (drawData != nullptr) {
        imguiVertices = static_cast<std::uint32_t>(std::max(0, drawData->TotalVtxCount));
        for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex) {
          imguiDrawCalls +=
            static_cast<std::uint32_t>(drawData->CmdLists[listIndex]->CmdBuffer.Size);
        }
      }
      stats.drawCalls = 4 + imguiDrawCalls;
      stats.triangles = 1 + cubeIndexCount / 3 + 2 + imguiVertices / 3;
      stats.renderPasses = 5;
    } else {
      stats.drawCalls = 4;
      stats.triangles = 1 + cubeIndexCount / 3 + 2;
      stats.renderPasses = 4;
    }
    stats.lines = gridVertexCount / 2;

    vkCmdEndRendering(commandBuffer);

    if (windowFrame) {
      imageBarrier(commandBuffer, targetImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                   VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
      swapchainLayouts[currentImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    } else {
      imageBarrier(commandBuffer, targetImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                   VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                   VK_PIPELINE_STAGE_2_COPY_BIT);

      VkBufferImageCopy copyRegion{};
      copyRegion.bufferOffset = 0;
      copyRegion.bufferRowLength = 0;
      copyRegion.bufferImageHeight = 0;
      copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.imageSubresource.mipLevel = 0;
      copyRegion.imageSubresource.baseArrayLayer = 0;
      copyRegion.imageSubresource.layerCount = 1;
      copyRegion.imageOffset = {0, 0, 0};
      copyRegion.imageExtent = {extent.width, extent.height, 1};
      vkCmdCopyImageToBuffer(commandBuffer, targetImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             readbackBuffer, 1, &copyRegion);
      offscreenLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    return check(vkEndCommandBuffer(commandBuffer), "Ending frame command buffer");
  }

  bool submitFrame() {
    Frame& frame = frames[currentFrame];

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (!headless) {
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = &frame.imageAvailable;
      submitInfo.pWaitDstStageMask = &waitStage;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &frame.renderFinished;
    }

    if (!check(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frame.fence), "Submitting frame")) {
      return false;
    }

    if (!headless) {
      VkPresentInfoKHR presentInfo{};
      presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pWaitSemaphores = &frame.renderFinished;
      presentInfo.swapchainCount = 1;
      presentInfo.pSwapchains = &swapchain.swapchain;
      presentInfo.pImageIndices = &frame.imageIndex;

      const VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
      if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        resizeRequested = true;
      } else if (!check(presentResult, "Presenting frame")) {
        return false;
      }
    }

    if (headless && renderedFrames == 0) {
      if (!check(vkQueueWaitIdle(graphicsQueue), "Waiting for first headless frame")) {
        return false;
      }
      vmaInvalidateAllocation(allocator, readbackAllocation, 0, VK_WHOLE_SIZE);
      const auto* pixels = static_cast<const std::uint8_t*>(readbackMapped);
      if (pixels != nullptr) {
        const std::size_t centerIndex =
          (static_cast<std::size_t>(extent.height) / 2U * extent.width + extent.width / 2U) * 4U;
        CRAB_LOG_INFO("rhi",
                      "Headless frame rendered; corner RGBA = {},{},{},{}; center RGBA = "
                      "{},{},{},{}",
                      pixels[0], pixels[1], pixels[2], pixels[3], pixels[centerIndex + 0],
                      pixels[centerIndex + 1], pixels[centerIndex + 2], pixels[centerIndex + 3]);
      }
    }

    ++renderedFrames;
    stats.frameIndex = renderedFrames;
    updateStats();
    return true;
  }

  bool beginFrame() {
    if (!initialized || frameActive || frameAborted) {
      return false;
    }

    if (!headless) {
      if (resizeRequested || !swapchainValid) {
        if (!recreateSwapchain()) {
          return false;
        }
      }
      if (!beginWindowFrame()) {
        return false;
      }
    } else {
      if (!beginHeadlessFrame()) {
        return false;
      }
    }

    if (imguiInitialized) {
      ImGui_ImplVulkan_NewFrame();
      if (imguiSdlInitialized) {
        ImGui_ImplSDL3_NewFrame();
      } else {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize =
          ImVec2(static_cast<float>(extent.width), static_cast<float>(extent.height));
        io.DeltaTime = 1.0F / 60.0F;
      }
      ImGui::NewFrame();
      imguiFrameActive = true;
      imguiFrameRendered = false;
    }

    frameActive = true;
    return true;
  }

  void drawDebugOverlay(const OverlayStats& overlayStats) {
    if (!imguiInitialized || !imguiFrameActive || !showOverlay) {
      return;
    }

    ImGui::SetNextWindowPos(ImVec2(12.0F, 12.0F), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0F, 0.0F), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Crab Debug")) {
      ImGui::TextUnformatted("Crab 0.1.0");
      ImGui::Separator();
      ImGui::Text("Frame: %llu", static_cast<unsigned long long>(overlayStats.frameIndex));
      ImGui::Text("Frame time: %.2f ms", overlayStats.frameTimeMs);
      ImGui::Text("FPS: %.1f", overlayStats.framesPerSecond);
      ImGui::Text("Resolution: %ux%u", extent.width, extent.height);
      ImGui::Text("Passes: %u", stats.renderPasses);
      ImGui::Text("Draw calls: %u", stats.drawCalls);
      ImGui::Text("Triangles: %u", stats.triangles);
      ImGui::Text("Lines: %u", stats.lines);
      ImGui::Text("Depth: %s", depthFormat == VK_FORMAT_D32_SFLOAT ? "D32_SFLOAT" : "D16_UNORM");
      ImGui::Separator();
      ImGui::Text("GPU: %s", deviceName.c_str());
      ImGui::Text("Validation: %s (%u errors)", validationEnabled ? "on" : "off",
                  stats.validationErrors);
      if (headless) {
        ImGui::TextUnformatted("Target: headless offscreen");
      } else {
        ImGui::Text("Present: FIFO, %u images", swapchainImageCount);
      }
      ImGui::Separator();
      ImGui::TextUnformatted("WASD/QE + Shift: fly camera");
      ImGui::TextUnformatted("F1: toggle this overlay");
      ImGui::TextUnformatted("Esc: quit");
    }
    ImGui::End();
  }

  bool endFrame() {
    if (!initialized || !frameActive) {
      return false;
    }

    if (!recordFrame()) {
      if (imguiFrameActive && !imguiFrameRendered) {
        ImGui::EndFrame();
      }
      imguiFrameActive = false;
      imguiFrameRendered = false;
      frameActive = false;
      frameAborted = true;
      return false;
    }

    if (!submitFrame()) {
      frameActive = false;
      imguiFrameActive = false;
      imguiFrameRendered = false;
      frameAborted = true;
      return false;
    }

    frameActive = false;
    imguiFrameActive = false;
    imguiFrameRendered = false;
    currentFrame = (currentFrame + 1) % kFramesInFlight;
    return true;
  }

  void processEvent(const SDL_Event& event) {
    if (imguiSdlInitialized) {
      ImGui_ImplSDL3_ProcessEvent(&event);
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F1) {
      showOverlay = !showOverlay;
    }

    const bool isKeyDown = event.type == SDL_EVENT_KEY_DOWN;
    const bool isKeyUp = event.type == SDL_EVENT_KEY_UP;
    if (!isKeyDown && !isKeyUp) {
      return;
    }
    const bool pressed = isKeyDown && !event.key.repeat;
    switch (event.key.key) {
    case SDLK_W:
      cameraMovement[0] = pressed;
      break;
    case SDLK_S:
      cameraMovement[1] = pressed;
      break;
    case SDLK_A:
      cameraMovement[2] = pressed;
      break;
    case SDLK_D:
      cameraMovement[3] = pressed;
      break;
    case SDLK_E:
      cameraMovement[4] = pressed;
      break;
    case SDLK_Q:
      cameraMovement[5] = pressed;
      break;
    case SDLK_LSHIFT:
      cameraFast = pressed;
      break;
    default:
      break;
    }
  }

  void requestResize(int width, int height) {
    if (headless) {
      return;
    }
    if (swapchainValid && width > 0 && height > 0 &&
        static_cast<std::uint32_t>(width) == extent.width &&
        static_cast<std::uint32_t>(height) == extent.height) {
      return;
    }
    resizeRequested = true;
  }

  void destroy() {
    if (deviceHandle != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(deviceHandle);
    }

    destroyImgui();

    if (pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(deviceHandle, pipeline, nullptr);
      pipeline = VK_NULL_HANDLE;
    }
    if (meshPipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(deviceHandle, meshPipeline, nullptr);
      meshPipeline = VK_NULL_HANDLE;
    }
    if (gridPipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(deviceHandle, gridPipeline, nullptr);
      gridPipeline = VK_NULL_HANDLE;
    }
    if (spritePipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(deviceHandle, spritePipeline, nullptr);
      spritePipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(deviceHandle, pipelineLayout, nullptr);
      pipelineLayout = VK_NULL_HANDLE;
    }
    if (descriptorPool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(deviceHandle, descriptorPool, nullptr);
      descriptorPool = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayout, nullptr);
      descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (textureSampler != VK_NULL_HANDLE) {
      vkDestroySampler(deviceHandle, textureSampler, nullptr);
      textureSampler = VK_NULL_HANDLE;
    }
    if (textureView != VK_NULL_HANDLE) {
      vkDestroyImageView(deviceHandle, textureView, nullptr);
      textureView = VK_NULL_HANDLE;
    }
    if (textureImage != VK_NULL_HANDLE) {
      vmaDestroyImage(allocator, textureImage, textureAllocation);
      textureImage = VK_NULL_HANDLE;
      textureAllocation = VK_NULL_HANDLE;
    }
    if (offscreenView != VK_NULL_HANDLE) {
      vkDestroyImageView(deviceHandle, offscreenView, nullptr);
      offscreenView = VK_NULL_HANDLE;
    }
    if (offscreenImage != VK_NULL_HANDLE) {
      vmaDestroyImage(allocator, offscreenImage, offscreenAllocation);
      offscreenImage = VK_NULL_HANDLE;
      offscreenAllocation = VK_NULL_HANDLE;
    }
    if (readbackMapped != nullptr) {
      vmaUnmapMemory(allocator, readbackAllocation);
      readbackMapped = nullptr;
    }
    if (readbackBuffer != VK_NULL_HANDLE) {
      vmaDestroyBuffer(allocator, readbackBuffer, readbackAllocation);
      readbackBuffer = VK_NULL_HANDLE;
      readbackAllocation = VK_NULL_HANDLE;
    }
    if (cubeIndexBuffer != VK_NULL_HANDLE) {
      vmaDestroyBuffer(allocator, cubeIndexBuffer, cubeIndexAllocation);
      cubeIndexBuffer = VK_NULL_HANDLE;
      cubeIndexAllocation = VK_NULL_HANDLE;
    }
    if (cubeVertexBuffer != VK_NULL_HANDLE) {
      vmaDestroyBuffer(allocator, cubeVertexBuffer, cubeVertexAllocation);
      cubeVertexBuffer = VK_NULL_HANDLE;
      cubeVertexAllocation = VK_NULL_HANDLE;
    }
    if (gridVertexBuffer != VK_NULL_HANDLE) {
      vmaDestroyBuffer(allocator, gridVertexBuffer, gridVertexAllocation);
      gridVertexBuffer = VK_NULL_HANDLE;
      gridVertexAllocation = VK_NULL_HANDLE;
    }
    destroyDepthResources();

    for (Frame& frame : frames) {
      if (frame.uniformMapped != nullptr) {
        vmaUnmapMemory(allocator, frame.uniformAllocation);
        frame.uniformMapped = nullptr;
      }
      if (frame.uniformBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, frame.uniformBuffer, frame.uniformAllocation);
        frame.uniformBuffer = VK_NULL_HANDLE;
        frame.uniformAllocation = VK_NULL_HANDLE;
      }
      if (frame.imageAvailable != VK_NULL_HANDLE) {
        vkDestroySemaphore(deviceHandle, frame.imageAvailable, nullptr);
        frame.imageAvailable = VK_NULL_HANDLE;
      }
      if (frame.renderFinished != VK_NULL_HANDLE) {
        vkDestroySemaphore(deviceHandle, frame.renderFinished, nullptr);
        frame.renderFinished = VK_NULL_HANDLE;
      }
      if (frame.fence != VK_NULL_HANDLE) {
        vkDestroyFence(deviceHandle, frame.fence, nullptr);
        frame.fence = VK_NULL_HANDLE;
      }
      frame.commandBuffer = VK_NULL_HANDLE;
      frame.descriptorSet = VK_NULL_HANDLE;
    }

    if (commandPool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(deviceHandle, commandPool, nullptr);
      commandPool = VK_NULL_HANDLE;
    }
    if (allocator != VK_NULL_HANDLE) {
      vmaDestroyAllocator(allocator);
      allocator = VK_NULL_HANDLE;
    }

    if (swapchainValid) {
      swapchain.destroy_image_views(swapchainImageViews);
      swapchainImageViews.clear();
      vkb::destroy_swapchain(swapchain);
      swapchainValid = false;
    }
    swapchainImages.clear();
    swapchainLayouts.clear();
    swapchainImageCount = 0;

    if (surface != VK_NULL_HANDLE) {
      SDL_Vulkan_DestroySurface(instanceHandle, surface, nullptr);
      surface = VK_NULL_HANDLE;
    }
    if (deviceHandle != VK_NULL_HANDLE) {
      vkb::destroy_device(device);
      device = {};
      deviceHandle = VK_NULL_HANDLE;
    }
    if (instanceHandle != VK_NULL_HANDLE) {
      vkb::destroy_instance(instance);
      instance = {};
      instanceHandle = VK_NULL_HANDLE;
    }

    initialized = false;
    frameActive = false;
    frameAborted = false;
    resizeRequested = false;
    swapchainValid = false;
    validationEnabled = false;
    imguiInitialized = false;
    imguiSdlInitialized = false;
    imguiFrameActive = false;
    imguiFrameRendered = false;
    headless = false;
    window = nullptr;
    currentFrame = 0;
    currentImageIndex = 0;
    renderedFrames = 0;
    extent = {0, 0};
    colorFormat = VK_FORMAT_UNDEFINED;
    depthFormat = VK_FORMAT_UNDEFINED;
    cubeIndexCount = 0;
    gridVertexCount = 0;
    deviceName.clear();
    stats = {};
    updateStats();
  }
};

Renderer::Renderer() : m_impl(std::make_unique<Impl>()) {}

Renderer::~Renderer() {
  shutdown();
}

bool Renderer::initialize(platform::Window& window, const RendererConfig& config) {
  if (m_impl->initialized) {
    return true;
  }
  m_impl->window = &window;
  if (m_impl->window->nativeHandle() == nullptr) {
    m_impl->error = "Cannot initialize the renderer with a null window";
    CRAB_LOG_ERROR("rhi", "{}", m_impl->error);
    m_impl->window = nullptr;
    return false;
  }
  if (!m_impl->initializeCommon(config, false)) {
    m_impl->destroy();
    return false;
  }
  return true;
}

bool Renderer::initializeHeadless(int width, int height, const RendererConfig& config) {
  if (m_impl->initialized) {
    return true;
  }
  if (width <= 0 || height <= 0) {
    m_impl->error = "Headless renderer dimensions must be positive";
    return false;
  }

  m_impl->window = nullptr;
  m_impl->extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
  if (!m_impl->initializeCommon(config, true)) {
    m_impl->destroy();
    return false;
  }
  return true;
}

void Renderer::shutdown() {
  if (m_impl != nullptr) {
    m_impl->destroy();
  }
}

void Renderer::waitIdle() {
  if (m_impl != nullptr && m_impl->deviceHandle != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_impl->deviceHandle);
  }
}

bool Renderer::isInitialized() const noexcept {
  return m_impl != nullptr && m_impl->initialized;
}

bool Renderer::isHeadless() const noexcept {
  return m_impl != nullptr && m_impl->headless;
}

bool Renderer::isFrameAborted() const noexcept {
  return m_impl != nullptr && m_impl->frameAborted;
}

const std::string& Renderer::lastError() const noexcept {
  static const std::string empty;
  return m_impl != nullptr ? m_impl->error : empty;
}

RendererStats Renderer::stats() const noexcept {
  return m_impl != nullptr ? m_impl->stats : RendererStats{};
}

bool Renderer::beginFrame() {
  return m_impl != nullptr && m_impl->beginFrame();
}

void Renderer::drawDebugOverlay(const OverlayStats& stats) {
  if (m_impl != nullptr) {
    m_impl->drawDebugOverlay(stats);
  }
}

bool Renderer::endFrame() {
  return m_impl != nullptr && m_impl->endFrame();
}

void Renderer::processEvent(const SDL_Event& event) {
  if (m_impl != nullptr) {
    m_impl->processEvent(event);
  }
}

void Renderer::requestResize(int width, int height) {
  if (m_impl != nullptr) {
    m_impl->requestResize(width, height);
  }
}

} // namespace crab::rhi
