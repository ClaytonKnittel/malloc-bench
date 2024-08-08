#pragma once

#include <cstddef>

#include "src/heap_interface.h"
#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {

class Mallocator {
 public:
  virtual void* malloc(size_t size) = 0;
};

/** Mallocator that thinly wraps a bench::Heap. */
class HeapMallocator : public Mallocator {
 public:
  explicit HeapMallocator(bench::Heap* heap) : heap_(*DCHECK_NON_NULL(heap)) {}

  void* malloc(size_t size) override {
    DCHECK_EQ(size % 16, 0);
    return heap_.sbrk(size);
  }

 private:
  bench::Heap& heap_;
};

/**
 * Mallocator for testing.
 *
 * Only supports one call to `malloc`,
 * and requires that the creator know an upper bound
 * on the size to be malloc'd.
 */
template <size_t N>
class StackMallactor : public Mallocator {
 public:
  void* malloc(size_t size) override {
    DCHECK_EQ(size % 16, 0);
    DCHECK_LE(size, N);
    return static_cast<void*>(data_);
  }

 private:
  uint8_t data_[N];
};

/** A StackMallactor that should be large enough for most testing. */
using BigStackMallocator = StackMallactor<1 << 16>;

}  // namespace jsmalloc
