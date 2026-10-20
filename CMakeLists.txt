cmake_minimum_required(VERSION 3.28)

project(Crab
  VERSION 0.1.0
  DESCRIPTION "A custom C++/Vulkan 2D/3D game engine"
  LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectOptions)
include(CompilerWarnings)
include(Sanitizers)

# ---------------------------------------------------------------------------
# Generated public headers
# ---------------------------------------------------------------------------
set(CRAB_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${CRAB_GENERATED_INCLUDE_DIR}/crab/core")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CRAB_BUILD_TYPE "Debug")
else()
  set(CRAB_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(CRAB_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
else()
  set(CRAB_COMPILER "${CMAKE_CXX_COMPILER_ID}")
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.hpp.in"
  "${CRAB_GENERATED_INCLUDE_DIR}/crab/core/version.hpp"
  @ONLY
)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
# Shaders are generated under the engine binary directory and copied next to
# the runtime so a build-tree executable can also be relocated with its shaders.
set(CRAB_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/engine/shaders")

add_subdirectory(third_party)
add_subdirectory(engine)
add_subdirectory(runtime)

if(CRAB_BUILD_EDITOR)
  message(WARNING "CRAB_BUILD_EDITOR is reserved for Phase 13; no editor target is built yet.")
endif()

if(CRAB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Crab ${PROJECT_VERSION} (${CRAB_BUILD_TYPE})")
message(STATUS "  Compiler:      ${CRAB_COMPILER}")
message(STATUS "  Build tests:   ${CRAB_BUILD_TESTS}")
message(STATUS "  Build editor:  ${CRAB_BUILD_EDITOR}")
message(STATUS "  Sanitizers:    ${CRAB_ENABLE_SANITIZERS}")
message(STATUS "  Tracy:         ${CRAB_ENABLE_TRACY}")
message(STATUS "  Warnings/err:  ${CRAB_ENABLE_WARNINGS_AS_ERRORS}")
message(STATUS "")
