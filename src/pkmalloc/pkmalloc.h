#pragma once

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/pkmalloc/global_state.h"

namespace pkmalloc {

class PkMalloc {
 public:
  // sbrk's a heap, initializes global pointers to heap and free list start
  static void initialize_heap(bench::HeapFactory& heap_factory);

  // return ptr to block of contiguous memory >= size
  // either sbrk new memory or pull block from free list
  static void* malloc(size_t size, size_t alignment = 0);

  static void* calloc(size_t nmemb, size_t size);

  static void* realloc(void* ptr, size_t size);

  // frees allocated memory, updates free list
  static void free(void* ptr, size_t size_hint = 0, size_t alignment_hint = 0);

  static size_t get_size(void* ptr);

 private:
};

}  // namespace pkmalloc