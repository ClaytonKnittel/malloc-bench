#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "src/ckmalloc/ckmalloc.h"

namespace bench {

inline void* malloc(size_t size, size_t alignment = 0) {
  return ckmalloc::CkMalloc::Instance()->Malloc(size, alignment);
}

inline void* calloc(size_t nmemb, size_t size) {
  return ckmalloc::CkMalloc::Instance()->Calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  return ckmalloc::CkMalloc::Instance()->Realloc(ptr, size);
}

inline void free(void* ptr, size_t size = 0, size_t alignment = 0) {
  ckmalloc::CkMalloc::Instance()->Free(ptr, size, alignment);
}

inline size_t get_size(void* ptr) {
  return ckmalloc::CkMalloc::Instance()->GetSize(ptr);
}

}  // namespace bench
