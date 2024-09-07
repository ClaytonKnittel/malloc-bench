#pragma once

#include <cstdlib>
#include <cstring>

#include "src/jsmalloc/jsmalloc.h"
#include "src/heap_factory.h"

namespace bench {

static constexpr size_t kHeapSize = 512 * (1 << 20);

extern HeapFactory* g_heap_factory;
extern size_t g_heap_idx;

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  jsmalloc::initialize_heap(heap_factory);
}

inline void* malloc(size_t size) {
  return jsmalloc::malloc(size);
}

inline void* calloc(size_t nmemb, size_t size) {
  return jsmalloc::calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  return jsmalloc::realloc(ptr, size);
}

inline void free(void* ptr) {
  return jsmalloc::free(ptr);
}

}  // namespace bench
