add_executable(crab_runtime src/main.cpp)

set_target_properties(crab_runtime PROPERTIES
  OUTPUT_NAME crab
  WIN32_EXECUTABLE OFF
)

target_link_libraries(crab_runtime PRIVATE crab::engine)
target_compile_features(crab_runtime PRIVATE cxx_std_20)
crab_set_warnings(crab_runtime)
crab_enable_sanitizers(crab_runtime)
add_dependencies(crab_runtime crab_shaders)

# Keep the SPIR-V files next to the executable as a fallback for builds that
# are moved or packaged without the compile-time CRAB_SHADER_DIR path.
add_custom_command(TARGET crab_runtime POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${CRAB_SHADER_OUTPUT_DIR}" "$<TARGET_FILE_DIR:crab_runtime>/shaders"
  COMMENT "Copying SPIR-V shaders next to the runtime"
  VERBATIM
)

install(TARGETS crab_runtime
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY "${CRAB_SHADER_OUTPUT_DIR}/"
  DESTINATION ${CMAKE_INSTALL_BINDIR}/shaders
)
