#include "src/ckmalloc/ckmalloc.h"

#include <cstddef>
#include <new>

#include "src/ckmalloc/allocator.h"
#include "src/singleton_heap.h"

namespace ckmalloc {

namespace {

constexpr size_t kAlignment = 16;

Allocator alloc = []() {
  return Allocator(bench::SingletonHeap::GlobalInstance());
}();

// Rounds a size up to the largest size which is treated equally as this one.
size_t AlignSize(size_t size) {
  if (size <= 8) {
    return 8;
  }
  return (size + kAlignment - 1) & ~(kAlignment - 1);
}

// Returns the alignment that should be used for a given size.
std::align_val_t AlignmentForSize(size_t size) {
  if (size <= 8) {
    return static_cast<std::align_val_t>(8);
  }

  return static_cast<std::align_val_t>(16);
}

}  // namespace

void* malloc(size_t size) {
  return alloc.Alloc(AlignSize(size), AlignmentForSize(size));
}

void* calloc(size_t nmemb, size_t size) {
  void* block = malloc(nmemb * size);
  if (block != nullptr) {
    memset(block, 0, nmemb * size);
  }
  return block;
}

void* realloc(void* ptr, size_t size) {
  void* new_ptr = malloc(size);
  if (new_ptr != nullptr) {
    memcpy(new_ptr, ptr, size);
  }
  return new_ptr;
}

void free(void* ptr) {}

}  // namespace ckmalloc
