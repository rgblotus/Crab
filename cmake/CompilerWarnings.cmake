include_guard(GLOBAL)

# Applies the engine's warning policy to first-party targets only.
# Third-party headers are included as SYSTEM where possible.
function(crab_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4
      /permissive-
      /Zc:__cplusplus
      /Zc:preprocessor
      /utf-8
    )
    if(CRAB_ENABLE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wnon-virtual-dtor
      -Woverloaded-virtual
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
      -Wimplicit-fallthrough
      -Wcast-align
      -Wundef
    )
    if(CRAB_ENABLE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${target} PRIVATE -fno-omit-frame-pointer)
    endif()
  endif()
endfunction()
