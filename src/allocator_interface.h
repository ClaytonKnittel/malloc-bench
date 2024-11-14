#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>

#include "src/pkmalloc/pkmalloc.h"

namespace bench {

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  pkmalloc::PkMalloc::initialize_heap(heap_factory);
}

// return ptr to block of contiguous memory >= size
// either sbrk new memory or pull block from free list
inline void* malloc(size_t size, size_t alignment = 0) {
  return pkmalloc::PkMalloc::malloc(size, alignment);
}

inline void* calloc(size_t nmemb, size_t size) {
  return pkmalloc::PkMalloc::calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  return pkmalloc::PkMalloc::realloc(ptr, size);
}

// frees allocated memory, updates free list
inline void free(void* ptr, size_t size_hint = 0, size_t alignment_hint = 0) {
  pkmalloc::PkMalloc::free(ptr, size_hint, alignment_hint);
}

inline size_t get_size(void* ptr) {
  return pkmalloc::PkMalloc::get_size(ptr);
}

}  // namespace bench
