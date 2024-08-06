#pragma once

#include <cstddef>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// The leaf size should be roughly the square root of heap size / page size.
// Round down the leaf sizes so they are smaller.
constexpr uint32_t kNodeShift = (kHeapSizeShift - kPageShift + 2) / 3;
// The number of pages in the leaf nodes of the slab map.
constexpr size_t kNodeSize = 1 << kNodeShift;

constexpr uint32_t kRootShift = kHeapSizeShift - kPageShift - 2 * kNodeShift;
// The length of the root node in the slab map.
constexpr size_t kRootSize = 1 << kRootShift;

template <AllocFn Alloc>
class SlabMapImpl {
  friend class SlabMapTest;

 public:
  // Returns the slab metadata for a given slab id. Returns `nullptr` if no
  // metadata has ever been allocated for this slab id.
  Slab* FindSlab(SlabId slab_id) const;

  // Ensures that the slab map has allocated the necessary nodes for an entry
  // between start and end slab id's.
  absl::Status AllocatePath(SlabId start_id, SlabId end_id);

  // Inserts an association from `slab_id` to `slab`.
  void Insert(SlabId slab_id, Slab* slab);

  // Inserts an association from all slabs between start_id and end_id
  // (inclusive) to `slab`.
  void InsertRange(SlabId start_id, SlabId end_id, Slab* slab);

 private:
  class Leaf {
   public:
    Slab* operator[](size_t idx) const {
      CK_ASSERT(idx < kNodeSize);
      return slabs_[idx];
    }

    void SetLeaf(size_t idx, Slab* slab) {
      slabs_[idx] = slab;
    }

   private:
    Slab* slabs_[kNodeSize] = {};
  };

  class Node {
    friend class SlabMapTest;

   public:
    Leaf* operator[](size_t idx) const {
      CK_ASSERT(idx < kNodeSize);
      return leaves_[idx];
    }

    absl::StatusOr<Leaf*> GetOrAllocateLeaf(size_t idx);

   private:
    Leaf* leaves_[kNodeSize] = {};
  };

  template <typename T>
  static T* Allocate() {
    T* ptr = static_cast<T*>(Alloc(sizeof(T), alignof(T)));
    new (ptr) T();
    return ptr;
  }

  static size_t RootIdx(SlabId slab_id) {
    return slab_id.Idx() / (kNodeSize * kNodeSize);
  }

  static size_t MiddleIdx(SlabId slab_id) {
    return (slab_id.Idx() / kNodeSize) % kRootSize;
  }

  static size_t LeafIdx(SlabId slab_id) {
    return slab_id.Idx() % kNodeSize;
  }

  Node* operator[](size_t idx) const {
    CK_ASSERT(idx < kRootSize);
    return nodes_[idx];
  }

  absl::StatusOr<Node*> GetOrAllocateNode(size_t idx);

  Node* nodes_[kRootSize] = {};
};

template <AllocFn Alloc>
Slab* SlabMapImpl<Alloc>::FindSlab(SlabId slab_id) const {
  size_t root_idx = RootIdx(slab_id);
  size_t middle_idx = MiddleIdx(slab_id);
  size_t leaf_idx = LeafIdx(slab_id);

  Node* node = (*this)[root_idx];
  if (node == nullptr) {
    return nullptr;
  }

  Leaf* leaf = (*node)[middle_idx];
  if (leaf == nullptr) {
    return nullptr;
  }

  return (*leaf)[leaf_idx];
}

template <AllocFn Alloc>
absl::Status SlabMapImpl<Alloc>::AllocatePath(SlabId start_id, SlabId end_id) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  auto middle_idxs = std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));

  std::cout << "Ranging " << root_idxs.first << "," << root_idxs.second << " "
            << middle_idxs.first << "," << middle_idxs.second << std::endl;
  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    DEFINE_OR_RETURN(Node*, node, GetOrAllocateNode(root_idx));
    for (size_t middle_idx = root_idx != root_idxs.first ? 0
                                                         : middle_idxs.first;
         middle_idx <=
         (root_idx != root_idxs.second ? kNodeSize : middle_idxs.second);
         middle_idx++) {
      RETURN_IF_ERROR(node->GetOrAllocateLeaf(middle_idx).status());
      std::cerr << "Allocated " << root_idx << " " << middle_idx << std::endl;
    }
  }

  return absl::OkStatus();
}

template <AllocFn Alloc>
void SlabMapImpl<Alloc>::Insert(SlabId slab_id, Slab* slab) {
  Node& node = *(*this)[RootIdx(slab_id)];
  Leaf& leaf = *node[MiddleIdx(slab_id)];
  leaf.SetLeaf(LeafIdx(slab_id), slab);
}

template <AllocFn Alloc>
void SlabMapImpl<Alloc>::InsertRange(SlabId start_id, SlabId end_id,
                                     Slab* slab) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  auto middle_idxs = std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));
  auto leaf_idxs = std::make_pair(LeafIdx(start_id), LeafIdx(end_id));

  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    Node& node = *(*this)[root_idx];
    for (size_t middle_idx = root_idx != root_idxs.first ? 0
                                                         : middle_idxs.first;
         middle_idx <=
         (root_idx != root_idxs.second ? kNodeSize : middle_idxs.second);
         middle_idx++) {
      Leaf& leaf = *node[middle_idx];
      for (size_t leaf_idx =
               (root_idx != root_idxs.first || middle_idx != middle_idxs.first
                    ? 0
                    : leaf_idxs.first);
           leaf_idx <=
           (root_idx != root_idxs.second || middle_idx != middle_idxs.second
                ? kNodeSize
                : leaf_idxs.second);
           leaf_idx++) {
        leaf.SetLeaf(leaf_idx, slab);
      }
    }
  }
}

template <AllocFn Alloc>
absl::StatusOr<typename SlabMapImpl<Alloc>::Leaf*>
SlabMapImpl<Alloc>::Node::GetOrAllocateLeaf(size_t idx) {
  if (leaves_[idx] == nullptr) {
    leaves_[idx] = Allocate<Leaf>();
    if (leaves_[idx] == nullptr) {
      return absl::ResourceExhaustedError("Out of memory");
    }
  }

  return leaves_[idx];
}

template <AllocFn Alloc>
absl::StatusOr<typename SlabMapImpl<Alloc>::Node*>
SlabMapImpl<Alloc>::GetOrAllocateNode(size_t idx) {
  if (nodes_[idx] == nullptr) {
    nodes_[idx] = Allocate<Node>();
    if (nodes_[idx] == nullptr) {
      return absl::ResourceExhaustedError("Out of memory");
    }
  }

  return nodes_[idx];
}

using SlabMap = SlabMapImpl<MetadataAlloc>;

}  // namespace ckmalloc
