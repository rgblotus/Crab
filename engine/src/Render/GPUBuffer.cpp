#include <Crab/Render/GPUBuffer.h>
#include <Crab/Render/VulkanContext.h>
#include <cstring>

namespace Crab {

GPUBuffer::GPUBuffer(GPUBuffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_memory(other.m_memory)
    , m_size(other.m_size)
{
    other.m_buffer = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_size = 0;
}

GPUBuffer& GPUBuffer::operator=(GPUBuffer&& other) noexcept {
    if (this != &other) {
        m_buffer = other.m_buffer;
        m_memory = other.m_memory;
        m_size = other.m_size;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_memory = VK_NULL_HANDLE;
        other.m_size = 0;
    }
    return *this;
}

GPUBuffer::~GPUBuffer() {
    // Must call destroy() explicitly
}

bool GPUBuffer::create(const VulkanContext& ctx, VkDeviceSize size,
                        VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                        const void* data)
{
    VkDevice device = ctx.device();
    m_size = size;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, m_buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = ctx.findMemoryType(memReq.memoryTypeBits, props);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(device, m_buffer, m_memory, 0);

    // Upload initial data if provided
    if (data) {
        upload(device, data, size);
    }

    return true;
}

void GPUBuffer::destroy(VkDevice device) {
    if (m_memory) vkFreeMemory(device, m_memory, nullptr);
    if (m_buffer) vkDestroyBuffer(device, m_buffer, nullptr);
    m_buffer = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_size = 0;
}

void GPUBuffer::upload(VkDevice device, const void* data, VkDeviceSize size) const {
    void* mapped = map(device);
    if (mapped) {
        std::memcpy(mapped, data, size);
        unmap(device);
    }
}

void* GPUBuffer::map(VkDevice device) const {
    void* data;
    if (vkMapMemory(device, m_memory, 0, m_size, 0, &data) == VK_SUCCESS) {
        return data;
    }
    return nullptr;
}

void GPUBuffer::unmap(VkDevice device) const {
    vkUnmapMemory(device, m_memory);
}

} // namespace Crab
