#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/util.h"
#include "src/singleton_heap.h"

namespace ckmalloc {

// NOLINTBEGIN(google-readability-casting)

constexpr uint32_t kHeapSizeShift = 29;
static_assert(bench::SingletonHeap::kHeapSize == (size_t(1) << kHeapSizeShift));

// The leaf size should be roughly the square root of heap size / page size.
// Round down the leaf sizes so they are smaller.
constexpr uint32_t kNodeShift = (kHeapSizeShift - kPageShift + 2) / 3;
// The number of pages in the leaf nodes of the slab map.
constexpr size_t kNodeSize = 1 << kNodeShift;

constexpr uint32_t kRootShift = kHeapSizeShift - kPageShift - 2 * kNodeShift;
// The length of the root node in the slab map.
constexpr size_t kRootSize = 1 << kRootShift;

// NOLINTEND(google-readability-casting)

// This method is defined in state.cc, but we can't depend on :state due to
// circular dependencies.
extern void* MetadataAlloc(size_t size, std::align_val_t alignment);

class SlabMap {
 public:
  // Ensures that the slab map has allocated the necessary nodes for an entry
  // between start and end slab id's.
  void AllocatePath(SlabId start_id, SlabId end_id);

 private:
  class Leaf {
   public:
    Slab& operator[](size_t idx) {
      CK_ASSERT(idx < kNodeSize);
      CK_ASSERT(slabs_[idx] != nullptr);
      return *slabs_[idx];
    }

   private:
    Slab* slabs_[kNodeSize];
  };

  class Node {
   public:
    Leaf& operator[](size_t idx) {
      CK_ASSERT(idx < kNodeSize);
      CK_ASSERT(leaves_[idx] != nullptr);
      return *leaves_[idx];
    }

   private:
    Leaf* leaves_[kNodeSize];
  };

  static size_t RootIdx(SlabId slab_id) {
    return slab_id.Idx() / (kNodeSize * kNodeSize);
  }

  static size_t MiddleIdx(SlabId slab_id) {
    return (slab_id.Idx() / kNodeSize) % kRootSize;
  }

  static size_t LeafIdx(SlabId slab_id) {
    return slab_id.Idx() % (kRootSize * kNodeSize);
  }

  Node& operator[](size_t idx) {
    CK_ASSERT(idx < kRootSize);
    CK_ASSERT(nodes_[idx] != nullptr);
    return *nodes_[idx];
  }

  Node* nodes_[kRootSize];
};

}  // namespace ckmalloc
