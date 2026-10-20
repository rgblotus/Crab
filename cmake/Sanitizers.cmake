include_guard(GLOBAL)

function(crab_enable_sanitizers target)
  if(NOT CRAB_ENABLE_SANITIZERS)
    return()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined
      -fno-sanitize-recover=all
      -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE
      -fsanitize=address,undefined
    )
  else()
    message(WARNING "Sanitizers are only configured for GCC/Clang; ignoring for ${target}")
  endif()
endfunction()
