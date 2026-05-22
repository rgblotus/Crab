#include <Crab/Render/Shader.h>
#include <shaderc/shaderc.hpp>
#include <fmt/core.h>
#include <fstream>
#include <vector>

namespace Crab {

static shaderc_shader_kind toShadercKind(VkShaderStageFlagBits stage) {
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:           return shaderc_vertex_shader;
        case VK_SHADER_STAGE_FRAGMENT_BIT:         return shaderc_fragment_shader;
        case VK_SHADER_STAGE_COMPUTE_BIT:          return shaderc_compute_shader;
        case VK_SHADER_STAGE_GEOMETRY_BIT:         return shaderc_geometry_shader;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:  return shaderc_tess_control_shader;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return shaderc_tess_evaluation_shader;
        default: return shaderc_glsl_infer_from_source;
    }
}

std::vector<uint32_t> Shader::compileGLSL(const std::string& source, VkShaderStageFlagBits stage) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    auto result = compiler.CompileGlslToSpv(source, toShadercKind(stage), "shader", options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        fmt::println("Shader compile error: {}", result.GetErrorMessage());
        return {};
    }
    return {result.cbegin(), result.cend()};
}

VkShaderStageFlagBits Shader::stageFromFileExt(const std::string& filepath) {
    auto dot = filepath.rfind('.');
    if (dot == std::string::npos) return VK_SHADER_STAGE_VERTEX_BIT;
    std::string ext = filepath.substr(dot);
    if (ext == ".vert") return VK_SHADER_STAGE_VERTEX_BIT;
    if (ext == ".frag") return VK_SHADER_STAGE_FRAGMENT_BIT;
    if (ext == ".comp") return VK_SHADER_STAGE_COMPUTE_BIT;
    if (ext == ".geom") return VK_SHADER_STAGE_GEOMETRY_BIT;
    if (ext == ".tesc") return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (ext == ".tese") return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    return VK_SHADER_STAGE_VERTEX_BIT;
}

bool Shader::loadFromFile(VkDevice device, const std::string& filepath, VkShaderStageFlagBits stage) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return compile(device, source, stage);
}

bool Shader::loadFromFile(VkDevice device, const std::string& filepath) {
    return loadFromFile(device, filepath, stageFromFileExt(filepath));
}

Shader::Shader(Shader&& other) noexcept
    : m_module(other.m_module), m_stage(other.m_stage) {
    other.m_module = VK_NULL_HANDLE;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        m_module = other.m_module;
        m_stage = other.m_stage;
        other.m_module = VK_NULL_HANDLE;
    }
    return *this;
}

Shader::~Shader() {
    // Note: destroy() must be called before destruction with a valid device
}

bool Shader::compile(VkDevice device, const std::string& source, VkShaderStageFlagBits stage) {
    m_stage = stage;
    auto spirv = compileGLSL(source, stage);
    if (spirv.empty()) return false;

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spirv.size() * sizeof(uint32_t);
    info.pCode = spirv.data();

    return vkCreateShaderModule(device, &info, nullptr, &m_module) == VK_SUCCESS;
}

void Shader::destroy(VkDevice device) {
    if (m_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_module, nullptr);
        m_module = VK_NULL_HANDLE;
    }
}

} // namespace Crab
