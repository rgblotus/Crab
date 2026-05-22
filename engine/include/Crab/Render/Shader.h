#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <string>

namespace Crab {

class VulkanContext;

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool compile(VkDevice device, const std::string& source, VkShaderStageFlagBits stage);
    bool loadFromFile(VkDevice device, const std::string& filepath, VkShaderStageFlagBits stage);
    bool loadFromFile(VkDevice device, const std::string& filepath);
    void destroy(VkDevice device);

    VkShaderModule module() const { return m_module; }
    VkShaderStageFlagBits stage() const { return m_stage; }

    static std::vector<uint32_t> compileGLSL(const std::string& source, VkShaderStageFlagBits stage);
    static VkShaderStageFlagBits stageFromFileExt(const std::string& filepath);

private:
    VkShaderModule m_module = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_stage{};
};

} // namespace Crab
