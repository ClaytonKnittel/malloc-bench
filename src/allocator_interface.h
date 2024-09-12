#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace bench {

static constexpr size_t kHeapSize = 512 * (1 << 20);

extern Heap* g_heap;

extern std::mutex g_lock;

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  auto res = heap_factory.NewInstance(kHeapSize);
  if (!res.ok()) {
    std::cerr << "Failed to initialize heap" << std::endl;
    std::exit(-1);
  }
  g_heap = res.value().second;
}

void initialize();

inline void* malloc(size_t size, size_t alignment = 0) {
  std::lock_guard<std::mutex> lock(g_lock);
  if (g_heap == nullptr) {
    initialize();
  }

  // TODO: implement
  (void) alignment;
  if (size == 0) {
    return nullptr;
  }
  size_t round_up = (size + 0xf) & ~0xf;
  return g_heap->sbrk(round_up);
}

inline void* calloc(size_t nmemb, size_t size) {
  // TODO: implement
  void* ptr = malloc(nmemb * size);
  if (ptr != nullptr) {
    memset(ptr, 0, nmemb * size);
  }
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

inline void free(void* ptr, size_t size = 0, size_t align = 0) {
  // TODO: implement
}

inline size_t get_size(void* ptr) {
  // TODO: implement
  (void) ptr;
  return 0;
}

}  // namespace bench
