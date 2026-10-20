add_executable(crab_tests
  unit/application/test_application.cpp
  unit/core/test_log.cpp
  unit/core/test_version.cpp
  unit/rhi/test_camera.cpp
  unit/rhi/test_renderer.cpp
)

target_link_libraries(crab_tests
  PRIVATE
    crab::engine
    Catch2::Catch2WithMain
)

target_compile_features(crab_tests PRIVATE cxx_std_20)
crab_set_warnings(crab_tests)
crab_enable_sanitizers(crab_tests)

include("${CRAB_CATCH2_EXTRAS_DIR}/Catch.cmake")
catch_discover_tests(crab_tests)

add_test(NAME crab_headless_smoke COMMAND crab_runtime --headless --frames 3)
set_tests_properties(crab_headless_smoke PROPERTIES TIMEOUT 30)

if(NOT DEFINED SDL_DUMMYVIDEO OR SDL_DUMMYVIDEO)
  add_test(NAME crab_window_smoke COMMAND crab_runtime --no-vulkan --frames 2)
  set_tests_properties(crab_window_smoke PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    TIMEOUT 30
  )
endif()

# Exercise the windowed swapchain/present path without a display. SDL's
# offscreen driver provides a VkSurfaceKHR, and Mesa lavapipe supplies a
# software Vulkan device. This is the same code path as a real window after
# surface creation. Skip it on machines without lavapipe.
find_file(CRAB_LAVAPIPE_ICD
  NAMES lvp_icd.json lvp_icd.x86_64.json lvp_icd.aarch64.json
  PATHS /usr/share/vulkan/icd.d /usr/local/share/vulkan/icd.d
)
if(CRAB_LAVAPIPE_ICD AND (NOT DEFINED SDL_OFFSCREEN OR SDL_OFFSCREEN))
  add_executable(crab_offscreen_render_test integration/offscreen_render.cpp)
  target_link_libraries(crab_offscreen_render_test PRIVATE crab::engine)
  target_compile_features(crab_offscreen_render_test PRIVATE cxx_std_20)
  crab_set_warnings(crab_offscreen_render_test)
  crab_enable_sanitizers(crab_offscreen_render_test)

  add_test(NAME crab_offscreen_vulkan_smoke COMMAND crab_runtime --frames 3)
  set_tests_properties(crab_offscreen_vulkan_smoke PROPERTIES
    ENVIRONMENT
      "SDL_VIDEODRIVER=offscreen;VK_ICD_FILENAMES=${CRAB_LAVAPIPE_ICD}"
    TIMEOUT 60
  )

  add_test(NAME crab_offscreen_resize_smoke COMMAND crab_offscreen_render_test)
  set_tests_properties(crab_offscreen_resize_smoke PROPERTIES
    ENVIRONMENT
      "SDL_VIDEODRIVER=offscreen;VK_ICD_FILENAMES=${CRAB_LAVAPIPE_ICD}"
    TIMEOUT 60
  )
endif()
