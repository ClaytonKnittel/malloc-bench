#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>

#include "src/pkmalloc/pkmalloc.h"

namespace bench {

// return ptr to block of contiguous memory >= size
// either sbrk new memory or pull block from free list
void* malloc(size_t size, size_t alignment = 0) {
  pkmalloc::PkMalloc::malloc(size, alignment);
}

void* calloc(size_t nmemb, size_t size) {
  pkmalloc::PkMalloc::calloc(nmemb, size);
}

void* realloc(void* ptr, size_t size) {
  pkmalloc::PkMalloc::realloc(ptr, size);
}

// frees allocated memory, updates free list
void free(void* ptr, size_t size_hint = 0, size_t alignment_hint = 0) {
  pkmalloc::PkMalloc::free(ptr, size_hint, alignment_hint);
}

size_t get_size(void* ptr) {
  pkmalloc::PkMalloc::get_size(ptr);
}

}  // namespace bench
