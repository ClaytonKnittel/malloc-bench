#pragma once

#include <cstdlib>
#include <new>

namespace bench {

inline void* malloc(size_t size) {
  return ::malloc(size);
}

inline void* calloc(size_t nmemb, size_t size) {
  return ::calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  return ::realloc(ptr, size);
}

inline void free(void* ptr) {
  return ::free(ptr);
}

inline void free_hint(void* ptr, std::align_val_t size) {
  return ::operator delete(ptr, size);
}

}  // namespace bench
