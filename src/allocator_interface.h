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

size_t static space_needed_with_header(const size_t& size) {
  // add size of header to size, 8 bytes
  size_t round_up = size + 0xff;
  // round up size of memory needed to be 16 byte aligned
  // zero out first four bits
  round_up = round_up & ~0xf;
  return round_up;
}

size_t static space_needed(const size_t& size) {
  // round up size of memory needed to be 16 byte aligned
  // zero out first four bits
  size_t round_up = size & ~0xf;
  return round_up;
}

Block* create_block_extend_heap(size_t size) {
  auto* block = reinterpret_cast<Block*>(
      SingletonHeap::GlobalInstance()->sbrk(space_needed_with_header(size)));
  block->SetBlockSize(size);
  block->SetFree(false);
  return block;
}

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  auto res = heap_factory.NewInstance(kHeapSize);
  if (!res.ok()) {
    std::cerr << "Failed to initialize heap" << std::endl;
    std::exit(-1);
  }
  g_heap = res.value();
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
  if (ptr != nullptr && new_ptr != nullptr) {
    memcpy(new_ptr, ptr, size);
  }
  return new_ptr;
}

inline void free(void* ptr, size_t size = 0, size_t alignment = 0) {
  // TODO: implement
}

inline size_t get_size(void* ptr) {
  // TODO: implement
  (void) ptr;
  return 0;
}

inline void free_hint(void* ptr, std::align_val_t size) {
  return ::operator delete(ptr, size);
}

}  // namespace bench
