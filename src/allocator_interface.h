#pragma once

#include <cstdlib>

namespace bench {

inline void* malloc(size_t size) {
  return ::malloc(size);
}

inline void free(void* ptr) {
  return ::free(ptr);
}

}  // namespace bench
