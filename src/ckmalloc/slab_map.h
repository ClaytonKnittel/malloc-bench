#pragma once

#include <cstddef>
#include <cstdint>

#include "src/ckmalloc/common.h"
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

template <AllocFn Alloc>
class SlabMapImpl {
 public:
  // Ensures that the slab map has allocated the necessary nodes for an entry
  // between start and end slab id's.
  void AllocatePath(SlabId start_id, SlabId end_id);

  // Inserts an association from `slab_id` to `slab`.
  void Insert(SlabId slab_id, Slab* slab);

  // Inserts an association from all slabs between start_id and end_id
  // (inclusive) to `slab`.
  void InsertRange(SlabId start_id, SlabId end_id, Slab* slab);

 private:
  class Leaf {
   public:
    Slab*& operator[](size_t idx) {
      CK_ASSERT(idx < kNodeSize);
      CK_ASSERT(slabs_[idx] != nullptr);
      return slabs_[idx];
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

    Leaf& GetOrAllocateLeaf(size_t idx);

   private:
    Leaf* leaves_[kNodeSize];
  };

  template <typename T>
  static T* Allocate() {
    return static_cast<T*>(Alloc(sizeof(T), alignof(T)));
  }

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

  Node& GetOrAllocateNode(size_t idx);

  Node* nodes_[kRootSize];
};

template <AllocFn Alloc>
void SlabMapImpl<Alloc>::AllocatePath(SlabId start_id, SlabId end_id) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  auto middle_idxs = std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));
  auto leaf_idxs = std::make_pair(LeafIdx(start_id), LeafIdx(end_id));

  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    Node& node = GetOrAllocateNode(root_idx);
    for (size_t middle_idx = root_idx != root_idxs.first ? 0
                                                         : middle_idxs.first;
         middle_idx <=
         (root_idx != root_idxs.second ? kNodeSize : middle_idxs.second);
         middle_idx++) {
      Leaf& leaf = node.GetOrAllocateLeaf(middle_idx);
      for (size_t leaf_idx =
               (root_idx != root_idxs.first || middle_idx != middle_idxs.first
                    ? 0
                    : leaf_idxs.first);
           leaf_idx <=
           (root_idx != root_idxs.second || middle_idx != middle_idxs.second
                ? kNodeSize
                : leaf_idxs.second);
           leaf_idx++) {
      }
    }
  }
}

template <AllocFn Alloc>
void SlabMapImpl<Alloc>::Insert(SlabId slab_id, Slab* slab) {
  (*this)[RootIdx(slab_id)][MiddleIdx(slab_id)][LeafIdx(slab_id)] = slab;
}

template <AllocFn Alloc>
void SlabMapImpl<Alloc>::InsertRange(SlabId start_id, SlabId end_id,
                                     Slab* slab) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  auto middle_idxs = std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));
  auto leaf_idxs = std::make_pair(LeafIdx(start_id), LeafIdx(end_id));

  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    Node& node = (*this)[root_idx];
    for (size_t middle_idx = root_idx != root_idxs.first ? 0
                                                         : middle_idxs.first;
         middle_idx <=
         (root_idx != root_idxs.second ? kNodeSize : middle_idxs.second);
         middle_idx++) {
      Leaf& leaf = node[middle_idx];
      for (size_t leaf_idx =
               (root_idx != root_idxs.first || middle_idx != middle_idxs.first
                    ? 0
                    : leaf_idxs.first);
           leaf_idx <=
           (root_idx != root_idxs.second || middle_idx != middle_idxs.second
                ? kNodeSize
                : leaf_idxs.second);
           leaf_idx++) {
        leaf[leaf_idx] = slab;
      }
    }
  }
}

template <AllocFn Alloc>
SlabMapImpl<Alloc>::Leaf& SlabMapImpl<Alloc>::Node::GetOrAllocateLeaf(
    size_t idx) {
  if (leaves_[idx] == nullptr) {
    leaves_[idx] = Allocate<Leaf>();
    // TODO: handle OOM
    CK_ASSERT(leaves_[idx] != nullptr);
  }

  return *leaves_[idx];
}

template <AllocFn Alloc>
SlabMapImpl<Alloc>::Node& SlabMapImpl<Alloc>::GetOrAllocateNode(size_t idx) {
  if (nodes_[idx] == nullptr) {
    nodes_[idx] = Allocate<Node>();
    // TODO: handle OOM
    CK_ASSERT(nodes_[idx] != nullptr);
  }

  return *nodes_[idx];
}

using SlabMap = SlabMapImpl<MetadataAlloc>;

}  // namespace ckmalloc
