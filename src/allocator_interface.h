#pragma once

#include <cstdlib>
#include <cstring>

#include "src/ckmalloc/ckmalloc.h"
#include "src/ckmalloc/state.h"
#include "src/heap_factory.h"

namespace bench {

static constexpr size_t kHeapSize = 512 * (1 << 20);

extern HeapFactory* g_heap_factory;
extern size_t g_heap_idx;

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  ckmalloc::State::InitializeWithEmptyHeap(&heap_factory);
}

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
