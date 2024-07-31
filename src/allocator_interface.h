#pragma once

#include <cstring>

#include "src/ckmalloc/ckmalloc.h"

namespace bench {

// Called before any allocations are made.
inline void initialize_heap() {}

inline void* malloc(size_t size) {
  return ckmalloc::malloc(size);
}

inline void* calloc(size_t nmemb, size_t size) {
  return ckmalloc::calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  return ckmalloc::realloc(ptr, size);
}

inline void free(void* ptr) {
  ckmalloc::free(ptr);
}

}  // namespace bench
