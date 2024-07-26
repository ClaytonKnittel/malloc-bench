#pragma once

#include <cstring>

#include "src/fake_heap.h"

namespace bench {

inline void* malloc(size_t size) {
  // TODO: implement
  if (size == 0) {
    return nullptr;
  }
  size_t round_up = (size + 0xf) & ~0xf;
  return FakeHeap::GlobalInstance()->sbrk(round_up);
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
  memcpy(new_ptr, ptr, size);
  return new_ptr;
}

inline void free(void* ptr) {
  // TODO: implement
}

}  // namespace bench
