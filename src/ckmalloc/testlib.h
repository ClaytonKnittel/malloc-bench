#pragma once

#include <cstddef>
#include <new>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

static void* Allocate(size_t size, size_t alignment) {
  return ::operator new(size, static_cast<std::align_val_t>(alignment));
}

using TestSlabMap = SlabMapImpl<Allocate>;

class TestHeap : public bench::Heap {
 public:
  static constexpr size_t kMaxNumPages = 64;
  static constexpr size_t kMaxHeapSize = kMaxNumPages * kPageSize;

  explicit TestHeap(uint64_t n_pages)
      : bench::Heap(&memory_region_,
                    std::min(n_pages * kPageSize, kMaxHeapSize)) {
    assert(n_pages <= kMaxNumPages);
  }

 private:
  // TODO: dynamically allocate
  uint8_t memory_region_[kMaxHeapSize];
};

}  // namespace ckmalloc
