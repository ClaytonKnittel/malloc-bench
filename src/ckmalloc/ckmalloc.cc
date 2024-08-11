#include "src/ckmalloc/ckmalloc.h"

#include <cstddef>
#include <cstring>

#include "src/ckmalloc/state.h"

namespace ckmalloc {

void* malloc(size_t size) {
  return State::Instance()->Freelist()->Alloc(size);
}

void* calloc(size_t nmemb, size_t size) {
  void* block = malloc(nmemb * size);
  if (block != nullptr) {
    memset(block, 0, nmemb * size);
  }
  return block;
}

void* realloc(void* ptr, size_t size) {
  return State::Instance()->Freelist()->Realloc(ptr, size);
}

void free(void* ptr) {
  State::Instance()->Freelist()->Free(ptr);
}

}  // namespace ckmalloc
