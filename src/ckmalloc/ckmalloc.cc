#include "src/ckmalloc/ckmalloc.h"

namespace ckmalloc {

void* malloc(size_t size) {
  return nullptr;
}

void* calloc(size_t nmemb, size_t size) {
  return nullptr;
}

void* realloc(void* ptr, size_t size) {
  return nullptr;
}

void free(void* ptr) {}

}  // namespace ckmalloc
