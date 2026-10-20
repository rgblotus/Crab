// VMA is header-only unless one translation unit defines this macro. Keeping
// the implementation isolated lets the engine use strict warnings for its own
// sources without suppressing warnings globally.
#define VMA_IMPLEMENTATION

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <vk_mem_alloc.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
