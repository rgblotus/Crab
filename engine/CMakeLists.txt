add_library(crab_core STATIC
  src/core/assert.cpp
  src/core/log.cpp
)

add_library(crab::core ALIAS crab_core)

target_include_directories(crab_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CRAB_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(crab_core
  PUBLIC
    spdlog::spdlog
)

if(CRAB_ENABLE_TRACY AND TARGET Tracy::TracyClient)
  target_link_libraries(crab_core PUBLIC Tracy::TracyClient)
endif()

target_compile_features(crab_core PUBLIC cxx_std_20)
crab_set_warnings(crab_core)
crab_enable_sanitizers(crab_core)

# ---------------------------------------------------------------------------
# Platform layer
# ---------------------------------------------------------------------------
add_library(crab_platform STATIC
  src/platform/platform.cpp
  src/platform/window.cpp
)

add_library(crab::platform ALIAS crab_platform)

target_include_directories(crab_platform
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CRAB_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(crab_platform
  PUBLIC
    crab::core
    SDL3::SDL3
)

target_compile_features(crab_platform PUBLIC cxx_std_20)
crab_set_warnings(crab_platform)
crab_enable_sanitizers(crab_platform)

# ---------------------------------------------------------------------------
# RHI (Vulkan 1.3 backend)
# ---------------------------------------------------------------------------
find_program(CRAB_GLSLC_EXECUTABLE
  NAMES glslc
  HINTS "${CMAKE_SOURCE_DIR}/.tools/bin"
  DOC "Path to glslc used to compile Crab shaders"
)
if(NOT CRAB_GLSLC_EXECUTABLE)
  message(FATAL_ERROR
    "glslc was not found. Install the 'glslc' package (Ubuntu: apt install glslc) "
    "or set CRAB_GLSLC_EXECUTABLE to a glslc binary."
  )
endif()

file(MAKE_DIRECTORY "${CRAB_SHADER_OUTPUT_DIR}")

set(CRAB_SHADER_SOURCES
  shaders/fullscreen.vert
  shaders/fullscreen.frag
  shaders/mesh.vert
  shaders/mesh.frag
  shaders/grid.vert
  shaders/grid.frag
  shaders/sprite.vert
  shaders/sprite.frag
)
set(CRAB_SHADER_OUTPUTS)
foreach(shader IN LISTS CRAB_SHADER_SOURCES)
  get_filename_component(shader_name "${shader}" NAME)
  set(shader_output "${CRAB_SHADER_OUTPUT_DIR}/${shader_name}.spv")
  add_custom_command(
    OUTPUT "${shader_output}"
    COMMAND "${CRAB_GLSLC_EXECUTABLE}" --target-env=vulkan1.3 -O
            "${CMAKE_CURRENT_SOURCE_DIR}/${shader}" -o "${shader_output}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
    COMMENT "Compiling ${shader_name}"
    VERBATIM
  )
  list(APPEND CRAB_SHADER_OUTPUTS "${shader_output}")
endforeach()

add_custom_target(crab_shaders DEPENDS ${CRAB_SHADER_OUTPUTS})

add_library(crab_vma STATIC
  src/rhi/vma_impl.cpp
)
target_link_libraries(crab_vma
  PRIVATE
    VulkanMemoryAllocator
    Vulkan::Headers
)
target_compile_definitions(crab_vma
  PRIVATE
    VK_NO_PROTOTYPES
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)
if(MSVC)
  target_compile_options(crab_vma PRIVATE /w)
else()
  target_compile_options(crab_vma PRIVATE -w)
endif()

add_library(crab_rhi STATIC
  src/rhi/camera.cpp
  src/rhi/vulkan_renderer.cpp
)

add_library(crab::rhi ALIAS crab_rhi)

target_include_directories(crab_rhi
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CRAB_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(crab_rhi
  PRIVATE
    crab::core
    crab::platform
    crab::imgui
    volk::volk
    vk-bootstrap::vk-bootstrap
    crab_vma
    VulkanMemoryAllocator
    Vulkan::Headers
    SDL3::SDL3
)

target_link_libraries(crab_rhi
  PUBLIC
    glm::glm
)

target_compile_definitions(crab_rhi
  PRIVATE
    VK_NO_PROTOTYPES
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
    CRAB_SHADER_DIR="${CRAB_SHADER_OUTPUT_DIR}"
)

target_compile_features(crab_rhi PUBLIC cxx_std_20)
crab_set_warnings(crab_rhi)
crab_enable_sanitizers(crab_rhi)
add_dependencies(crab_rhi crab_shaders)

# ---------------------------------------------------------------------------
# Engine runtime shell
# ---------------------------------------------------------------------------
add_library(crab_engine STATIC
  src/application.cpp
)

add_library(crab::engine ALIAS crab_engine)

target_include_directories(crab_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CRAB_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(crab_engine
  PUBLIC
    crab::core
    crab::platform
    crab::rhi
    glm::glm
)

target_compile_features(crab_engine PUBLIC cxx_std_20)
crab_set_warnings(crab_engine)
crab_enable_sanitizers(crab_engine)
