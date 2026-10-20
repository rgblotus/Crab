include_guard(GLOBAL)

include(FetchContent)

# CMake 4 removed compatibility with projects that still declare very old
# minimum versions. Several upstream dependencies are conservative; this keeps
# them configurable without patching.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
endif()

set(FETCHCONTENT_QUIET ON)
set(CRAB_DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------
set(CRAB_SDL3_VERSION "release-3.4.18" CACHE STRING "Pinned SDL3 tag")

if(CRAB_USE_SYSTEM_SDL3)
  find_package(SDL3 3.2 REQUIRED CONFIG)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDL_WERROR OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        ${CRAB_SDL3_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   FALSE
  )
  FetchContent_MakeAvailable(SDL3)
endif()

# ---------------------------------------------------------------------------
# spdlog (logging)
# ---------------------------------------------------------------------------
set(CRAB_SPDLOG_VERSION "v1.17.0" CACHE STRING "Pinned spdlog tag")
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        ${CRAB_SPDLOG_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   FALSE
)
FetchContent_MakeAvailable(spdlog)

# ---------------------------------------------------------------------------
# GLM (math)
# ---------------------------------------------------------------------------
set(CRAB_GLM_VERSION "1.0.3" CACHE STRING "Pinned GLM tag")

if(CRAB_USE_SYSTEM_GLM)
  find_package(glm REQUIRED CONFIG)
else()
  set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
  set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        ${CRAB_GLM_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   FALSE
  )
  FetchContent_MakeAvailable(glm)
endif()

# ---------------------------------------------------------------------------
# Catch2 (tests only)
# ---------------------------------------------------------------------------
if(CRAB_BUILD_TESTS)
  set(CRAB_CATCH2_VERSION "v3.16.1" CACHE STRING "Pinned Catch2 tag")
  set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(CATCH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(CATCH_BUILD_EXTRA_TESTS OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        ${CRAB_CATCH2_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   FALSE
  )
  FetchContent_MakeAvailable(Catch2)

  set(CRAB_CATCH2_EXTRAS_DIR "${catch2_SOURCE_DIR}/extras" CACHE INTERNAL "Catch2 CMake extras")
  list(APPEND CMAKE_MODULE_PATH "${CRAB_CATCH2_EXTRAS_DIR}")
endif()

# ---------------------------------------------------------------------------
# Vulkan headers (pinned independently of the system loader)
# ---------------------------------------------------------------------------
set(CRAB_VULKAN_HEADERS_VERSION "vulkan-sdk-1.4.363.0"
    CACHE STRING "Pinned Vulkan-Headers tag")

FetchContent_Declare(VulkanHeaders
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG        ${CRAB_VULKAN_HEADERS_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   FALSE
)
FetchContent_MakeAvailable(VulkanHeaders)

if(TARGET Vulkan-Headers)
  set_target_properties(Vulkan-Headers PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${VulkanHeaders_SOURCE_DIR}/include"
  )
endif()

# ---------------------------------------------------------------------------
# volk (Vulkan entry-point loader)
# ---------------------------------------------------------------------------
set(CRAB_VOLK_VERSION "vulkan-sdk-1.4.363.0" CACHE STRING "Pinned volk tag")
set(VOLK_INSTALL OFF CACHE BOOL "" FORCE)
set(VOLK_PULL_IN_VULKAN ON CACHE BOOL "" FORCE)

FetchContent_Declare(volk
  GIT_REPOSITORY https://github.com/zeux/volk.git
  GIT_TAG        ${CRAB_VOLK_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   FALSE
)
FetchContent_MakeAvailable(volk)

if(TARGET volk)
  set_target_properties(volk PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${volk_SOURCE_DIR}"
  )
endif()

# ---------------------------------------------------------------------------
# vk-bootstrap (device and swapchain selection)
# ---------------------------------------------------------------------------
set(CRAB_VK_BOOTSTRAP_VERSION "v1.4.365" CACHE STRING "Pinned vk-bootstrap tag")
set(VK_BOOTSTRAP_TEST OFF CACHE BOOL "" FORCE)
set(VK_BOOTSTRAP_INSTALL OFF CACHE BOOL "" FORCE)
set(VK_BOOTSTRAP_DISABLE_WARNINGS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(vk_bootstrap
  GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
  GIT_TAG        ${CRAB_VK_BOOTSTRAP_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   FALSE
)
FetchContent_MakeAvailable(vk_bootstrap)

# vk-bootstrap's public header is intentionally warning-clean for its own
# build settings, but the engine enables stricter warnings. Treat it as a
# system header so those warnings do not become engine build failures.
set_target_properties(vk-bootstrap PROPERTIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${vk_bootstrap_SOURCE_DIR}/src"
)

# ---------------------------------------------------------------------------
# Vulkan Memory Allocator (header-only)
# ---------------------------------------------------------------------------
set(CRAB_VMA_VERSION "v3.4.0" CACHE STRING "Pinned VulkanMemoryAllocator tag")
set(VMA_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(VulkanMemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG        ${CRAB_VMA_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   FALSE
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

set_target_properties(VulkanMemoryAllocator PROPERTIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${VulkanMemoryAllocator_SOURCE_DIR}/include"
)

# ---------------------------------------------------------------------------
# Dear ImGui (no upstream CMake project; build the core and the backends we use)
# ---------------------------------------------------------------------------
set(CRAB_IMGUI_VERSION "v1.93.0-docking" CACHE STRING "Pinned Dear ImGui tag")

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.30)
  # FetchContent_Populate(<name>) is the only supported way to acquire a
  # dependency that does not provide a CMakeLists.txt. Keep the old policy for
  # this intentionally small third-party shim.
  cmake_policy(SET CMP0169 OLD)
endif()

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        ${CRAB_IMGUI_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   FALSE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

add_library(crab_imgui STATIC
  "${imgui_SOURCE_DIR}/imgui.cpp"
  "${imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${imgui_SOURCE_DIR}/imgui_tables.cpp"
  "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
  "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
  "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)
add_library(crab::imgui ALIAS crab_imgui)

target_include_directories(crab_imgui
  SYSTEM PUBLIC
    $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
    $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/backends>
)

target_compile_features(crab_imgui PUBLIC cxx_std_17)
target_compile_definitions(crab_imgui
  PUBLIC
    IMGUI_IMPL_VULKAN_USE_VOLK
    VK_NO_PROTOTYPES
)
target_link_libraries(crab_imgui
  PUBLIC
    SDL3::SDL3
    volk::volk
    Vulkan::Headers
)

# ---------------------------------------------------------------------------
# Tracy (optional profiler)
# ---------------------------------------------------------------------------
if(CRAB_ENABLE_TRACY)
  set(CRAB_TRACY_VERSION "v0.14.1" CACHE STRING "Pinned Tracy tag")
  set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
  set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)

  FetchContent_Declare(tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG        ${CRAB_TRACY_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   FALSE
  )
  FetchContent_MakeAvailable(tracy)
endif()
