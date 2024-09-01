#pragma once

#include <cstdlib>
#include <cstring>

#include "src/heap_factory.h"

namespace bench {

static constexpr size_t kHeapSize = 512 * (1 << 20);

extern HeapFactory* g_heap_factory;
extern size_t g_heap_idx;

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  g_heap_factory = &heap_factory;
  g_heap_idx = 0;
  auto res = g_heap_factory->NewInstance(kHeapSize);
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
  void* result;
  while (true) {
    result = g_heap_factory->Instance(g_heap_idx)->sbrk(round_up);
    if (result == nullptr && g_heap_factory->NewInstance(kHeapSize).ok()) {
      g_heap_idx++;
      continue;
    }
    return result;
  }
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
