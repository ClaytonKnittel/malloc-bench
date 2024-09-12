#pragma once

#include <cstdlib>
#include <cstring>

#include "src/ckmalloc/ckmalloc.h"
#include "src/heap_factory.h"

namespace bench {

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  ckmalloc::CkMalloc::InitializeHeap(heap_factory);
}

inline void* malloc(size_t size) {
  return ckmalloc::CkMalloc::Instance()->Malloc(size);
}

inline void* calloc(size_t nmemb, size_t size) {
  return ckmalloc::CkMalloc::Instance()->Calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  return ckmalloc::CkMalloc::Instance()->Realloc(ptr, size);
}

inline void free(void* ptr) {
  ckmalloc::CkMalloc::Instance()->Free(ptr);
}

}  // namespace bench
