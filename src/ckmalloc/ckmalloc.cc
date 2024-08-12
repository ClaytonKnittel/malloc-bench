#include "src/ckmalloc/ckmalloc.h"

#include <cstddef>
#include <cstring>

#include "src/ckmalloc/state.h"

namespace ckmalloc {

void* malloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }
  return State::Instance()->MainAllocator()->Alloc(size);
}

void* calloc(size_t nmemb, size_t size) {
  void* block = malloc(nmemb * size);
  if (block != nullptr) {
    memset(block, 0, nmemb * size);
  }
  return block;
}

void* realloc(void* ptr, size_t size) {
  CK_ASSERT_NE(size, 0);
  if (ptr == nullptr) {
    return malloc(size);
  }
  return State::Instance()->MainAllocator()->Realloc(ptr, size);
}

void free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  State::Instance()->MainAllocator()->Free(ptr);
}

}  // namespace ckmalloc
