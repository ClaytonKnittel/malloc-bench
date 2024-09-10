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

// TODO: delete, make pure virtual interface.
class CkMallocInterface {
 public:
  static void initialize_heap(HeapFactory& heap_factory) {
    bench::initialize_heap(heap_factory);
  }

  static void* malloc(size_t size) {
    return bench::malloc(size);
  }

  static void* calloc(size_t nmemb, size_t size) {
    return bench::calloc(nmemb, size);
  }

  static void* realloc(void* ptr, size_t size) {
    return bench::realloc(ptr, size);
  }

  static void free(void* ptr) {
    return bench::free(ptr);
  }
};

}  // namespace bench
