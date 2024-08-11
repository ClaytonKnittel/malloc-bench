#pragma once

#include <cstddef>

#include "src/heap_interface.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {

class Allocator {
 public:
  virtual void* Allocate(size_t size) = 0;
  virtual void Free(void* ptr) = 0;
};

/** Mallocator that thinly wraps a bench::Heap. */
class HeapAllocator : public Allocator {
 public:
  explicit HeapAllocator(bench::Heap* heap) : heap_(*DCHECK_NON_NULL(heap)) {}

  void* Allocate(size_t size) override {
    DCHECK_EQ(size % 16, 0);
    return heap_.sbrk(size);
  }

  void Free(void* ptr) override {}

 private:
  bench::Heap& heap_;
};

/**
 * Allocator for testing.
 */
template <size_t N>
class StackAllocator : public Allocator {
 public:
  void* Allocate(size_t size) override {
    DCHECK_EQ(size % 16, 0);
    DCHECK_LE(size, N);
    if (end_ + size >= N) {
      return nullptr;
    }
    // Ensure we give out 16-byte aligned addresses.
    uint32_t initial_offset = 16 - (twiddle::PtrValue(this) % 16);
    void* ptr = static_cast<void*>(&data_[end_ + initial_offset]);
    end_ += size;
    return ptr;
  }

  void Free(void* ptr) override {}

 private:
  class Block {};

  size_t end_ = 0;
  uint8_t data_[N];
};

/** A StackAllocator that should be large enough for most testing. */
using BigStackAllocator = StackAllocator<1 << 16>;

}  // namespace jsmalloc
