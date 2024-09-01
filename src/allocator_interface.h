#pragma once

#include <cstdlib>
#include <cstring>

#include "src/heap_factory.h"

namespace bench {

extern HeapFactory* g_heap_factory;

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  g_heap_factory = &heap_factory;
  auto res = g_heap_factory->NewInstance(512 * (1 << 20));
  if (!res.ok()) {
    std::cerr << "Failed to initialize heap" << std::endl;
    std::exit(-1);
  }
}

inline void* malloc(size_t size) {
  // TODO: implement
  if (size == 0) {
    return nullptr;
  }
  size_t round_up = (size + 0xf) & ~0xf;
  return g_heap_factory->Instance(0)->sbrk(round_up);
}

inline void* calloc(size_t nmemb, size_t size) {
  // TODO: implement
  void* ptr = malloc(nmemb * size);
  memset(ptr, 0, nmemb * size);
  return ptr;
}

inline void* realloc(void* ptr, size_t size) {
  // TODO: implement
  void* new_ptr = malloc(size);
  if (ptr != nullptr) {
    memcpy(new_ptr, ptr, size);
  }
  return new_ptr;
}

inline void free(void* ptr) {
  // TODO: implement
}

}  // namespace bench
