cmake_minimum_required(VERSION 3.20)

find_package(fmt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

set(ENGINE_SRC
    src/Engine.cpp
    src/Core/Logger.cpp
    src/Platform/Window.cpp
    src/Platform/Input.cpp
    src/Render/VulkanContext.cpp
    src/Render/Shader.cpp
    src/Render/Pipeline.cpp
    src/Render/GPUBuffer.cpp
    src/Render/Camera.cpp
    src/Render/RenderMesh.cpp
    src/Render/Renderer.cpp
    src/Scene/Components.cpp
    src/Scene/Scene.cpp
)

add_library(CrabEngine SHARED ${ENGINE_SRC})

target_include_directories(CrabEngine PUBLIC include)

target_link_libraries(CrabEngine PUBLIC
    fmt::fmt
    glm::glm
    EnTT::EnTT
    unofficial::shaderc::shaderc
    SDL3::SDL3
    Vulkan::Vulkan
)

set_target_properties(CrabEngine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/engine"
)

add_custom_command(TARGET CrabEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_BINARY_DIR}/shaders"
    COMMENT "Copying shaders to build directory"
)
