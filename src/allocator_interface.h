#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "src/ckmalloc/ckmalloc.h"
#include "src/heap_factory.h"

namespace bench {

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  ckmalloc::CkMalloc::InitializeHeap(heap_factory);
}

inline void* malloc(size_t size, size_t alignment = 0) {
  (void) alignment;
  return ckmalloc::CkMalloc::Instance()->Malloc(size, alignment);
}

inline void* calloc(size_t nmemb, size_t size) {
  return ckmalloc::CkMalloc::Instance()->Calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  return ckmalloc::CkMalloc::Instance()->Realloc(ptr, size);
}

inline void free(void* ptr, size_t size = 0, size_t alignment = 0) {
  (void) size;
  (void) alignment;
  ckmalloc::CkMalloc::Instance()->Free(ptr, size, alignment);
}

inline size_t get_size(void* ptr) {
  return ckmalloc::CkMalloc::Instance()->GetSize(ptr);
}

}  // namespace bench
