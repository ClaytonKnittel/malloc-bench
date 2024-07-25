#pragma once

#include <cstdlib>
#include <new>

#include "src/fake_heap.h"

namespace bench {

inline void* malloc(size_t size) {
  // TODO: implement
  return ::malloc(size);
}

inline void* calloc(size_t nmemb, size_t size) {
  // TODO: implement
  return ::calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  // TODO: implement
  return ::realloc(ptr, size);
}

inline void free(void* ptr) {
  // TODO: implement
  return ::free(ptr);
}

inline void free_hint(void* ptr, std::align_val_t size) {
  // TODO: implement
  return ::operator delete(ptr, size);
}

// Should return a pointer to the heap manager currently being used. If this is
// not initialized, you should initialize it.
inline FakeHeap* HeapManager() {
  // TODO: implement
  return nullptr;
}

// Resets the heap. This should destroy the heap manager.
inline void ResetHeap() {
  // TODO: implement
}

}  // namespace bench
