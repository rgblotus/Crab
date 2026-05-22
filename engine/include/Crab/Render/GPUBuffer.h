#pragma once

#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>

namespace Crab {

class VulkanContext;

class GPUBuffer {
public:
    GPUBuffer() = default;
    ~GPUBuffer();

    GPUBuffer(GPUBuffer&& other) noexcept;
    GPUBuffer& operator=(GPUBuffer&& other) noexcept;
    GPUBuffer(const GPUBuffer&) = delete;
    GPUBuffer& operator=(const GPUBuffer&) = delete;

    bool create(const VulkanContext& ctx, VkDeviceSize size,
                VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                const void* data = nullptr);
    void destroy(VkDevice device);

    void upload(VkDevice device, const void* data, VkDeviceSize size) const;
    void* map(VkDevice device) const;
    void unmap(VkDevice device) const;

    VkBuffer handle() const { return m_buffer; }
    VkDeviceMemory memory() const { return m_memory; }
    VkDeviceSize size() const { return m_size; }

private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
};

} // namespace Crab
