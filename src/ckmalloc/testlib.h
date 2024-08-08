#pragma once

#include <cstddef>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class TestGlobalMetadataAlloc {
 public:
  static Slab* SlabAlloc();
  static void SlabFree(Slab* slab);
  static void* Alloc(size_t size, size_t alignment);
};

using TestSlabMap = SlabMapImpl<TestGlobalMetadataAlloc>;
using TestSlabManager = SlabManagerImpl<TestGlobalMetadataAlloc>;

class TestHeap : public bench::Heap {
 public:
  static constexpr size_t kMaxNumPages = 64;
  static constexpr size_t kMaxHeapSize = kMaxNumPages * kPageSize;

  explicit TestHeap(size_t n_pages)
      : bench::Heap(&memory_region_,
                    std::min(n_pages * kPageSize, kMaxHeapSize)) {
    assert(n_pages <= kMaxNumPages);
  }

 private:
  // TODO: dynamically allocate
  uint8_t memory_region_[kMaxHeapSize];
};

}  // namespace ckmalloc
