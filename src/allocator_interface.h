#pragma once

#include <cstring>

#include "src/jsmalloc/jsmalloc.h"
#include "src/singleton_heap.h"

namespace bench {

// Called before any allocations are made.
inline void initialize_heap() {
  jsmalloc::initialize_heap(*SingletonHeap::GlobalInstance());
}

inline void* malloc(size_t size) {
  return jsmalloc::malloc(*SingletonHeap::GlobalInstance(), size);
}

inline void* calloc(size_t nmemb, size_t size) {
  return jsmalloc::calloc(*SingletonHeap::GlobalInstance(), nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  return jsmalloc::realloc(*SingletonHeap::GlobalInstance(), ptr, size);
}

inline void free(void* ptr) {
  return jsmalloc::free(ptr);
}

}  // namespace bench
