#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "src/heap_factory.h"
#include "src/perfetto.h"  // IWYU pragma: keep

namespace bench {

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {}

void initialize();

inline void* malloc(size_t size, size_t alignment = 0) {
  (void) alignment;
  TRACE_EVENT("test_infrastructure", "Malloc");
  return ::malloc(size);
}

inline void* calloc(size_t nmemb, size_t size) {
  TRACE_EVENT("test_infrastructure", "Calloc");
  return ::calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  TRACE_EVENT("test_infrastructure", "Realloc");
  return ::realloc(ptr, size);
}

inline void free(void* ptr, size_t size = 0, size_t alignment = 0) {
  (void) size;
  (void) alignment;
  TRACE_EVENT("test_infrastructure", "Free");
  ::free(ptr);
}

inline size_t get_size(void* ptr) {
  // TODO: implement
  (void) ptr;
  return 0;
}

}  // namespace bench
