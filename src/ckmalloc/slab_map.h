#pragma once

#include <cstddef>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
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

template <MetadataAllocInterface MetadataAlloc>
class SlabMapImpl {
  friend class SlabMapTest;

 public:
  // Returns the slab metadata for a given slab id. Returns `nullptr` if no
  // metadata has ever been allocated for this slab id.
  Slab* FindSlab(PageId page_id) const;

  // Ensures that the slab map has allocated the necessary nodes for an entry
  // between start and end page id's.
  absl::Status AllocatePath(PageId start_id, PageId end_id);

  // Inserts an association from `page_id` to `slab`.
  void Insert(PageId page_id, Slab* slab);

  // Inserts an association from all pages between start_id and end_id
  // (inclusive) to `slab`.
  void InsertRange(PageId start_id, PageId end_id, Slab* slab);

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
    T* ptr = static_cast<T*>(MetadataAlloc::Alloc(sizeof(T), alignof(T)));
    new (ptr) T();
    return ptr;
  }

  static size_t RootIdx(PageId page_id) {
    return page_id.Idx() / (kNodeSize * kNodeSize);
  }

  static size_t MiddleIdx(PageId page_id) {
    return (page_id.Idx() / kNodeSize) % kNodeSize;
  }

  static size_t LeafIdx(PageId page_id) {
    return page_id.Idx() % kNodeSize;
  }

  Node* operator[](size_t idx) const {
    CK_ASSERT(idx < kRootSize);
    return nodes_[idx];
  }

  absl::StatusOr<Node*> GetOrAllocateNode(size_t idx);

  Node* nodes_[kRootSize] = {};
};

template <MetadataAllocInterface MetadataAlloc>
Slab* SlabMapImpl<MetadataAlloc>::FindSlab(PageId page_id) const {
  size_t root_idx = RootIdx(page_id);
  size_t middle_idx = MiddleIdx(page_id);
  size_t leaf_idx = LeafIdx(page_id);

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

template <MetadataAllocInterface MetadataAlloc>
absl::Status SlabMapImpl<MetadataAlloc>::AllocatePath(PageId start_id,
                                                      PageId end_id) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  auto middle_idxs = std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));

  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    DEFINE_OR_RETURN(Node*, node, GetOrAllocateNode(root_idx));
    for (size_t middle_idx =
             (root_idx != root_idxs.first ? 0 : middle_idxs.first);
         middle_idx <=
         (root_idx != root_idxs.second ? kNodeSize - 1 : middle_idxs.second);
         middle_idx++) {
      RETURN_IF_ERROR(node->GetOrAllocateLeaf(middle_idx).status());
    }
  }

  return absl::OkStatus();
}

template <MetadataAllocInterface MetadataAlloc>
void SlabMapImpl<MetadataAlloc>::Insert(PageId page_id, Slab* slab) {
  Node& node = *(*this)[RootIdx(page_id)];
  Leaf& leaf = *node[MiddleIdx(page_id)];
  leaf.SetLeaf(LeafIdx(page_id), slab);
}

template <MetadataAllocInterface MetadataAlloc>
void SlabMapImpl<MetadataAlloc>::InsertRange(PageId start_id, PageId end_id,
                                             Slab* slab) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  auto middle_idxs = std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));
  auto leaf_idxs = std::make_pair(LeafIdx(start_id), LeafIdx(end_id));

  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    Node& node = *(*this)[root_idx];
    for (size_t middle_idx =
             (root_idx != root_idxs.first ? 0 : middle_idxs.first);
         middle_idx <=
         (root_idx != root_idxs.second ? kNodeSize - 1 : middle_idxs.second);
         middle_idx++) {
      Leaf& leaf = *node[middle_idx];
      for (size_t leaf_idx =
               (root_idx != root_idxs.first || middle_idx != middle_idxs.first
                    ? 0
                    : leaf_idxs.first);
           leaf_idx <=
           (root_idx != root_idxs.second || middle_idx != middle_idxs.second
                ? kNodeSize - 1
                : leaf_idxs.second);
           leaf_idx++) {
        leaf.SetLeaf(leaf_idx, slab);
      }
    }
  }
}

template <MetadataAllocInterface MetadataAlloc>
absl::StatusOr<typename SlabMapImpl<MetadataAlloc>::Leaf*>
SlabMapImpl<MetadataAlloc>::Node::GetOrAllocateLeaf(size_t idx) {
  if (leaves_[idx] == nullptr) {
    leaves_[idx] = Allocate<Leaf>();
    if (leaves_[idx] == nullptr) {
      return absl::ResourceExhaustedError("Out of memory");
    }
  }

  return leaves_[idx];
}

template <MetadataAllocInterface MetadataAlloc>
absl::StatusOr<typename SlabMapImpl<MetadataAlloc>::Node*>
SlabMapImpl<MetadataAlloc>::GetOrAllocateNode(size_t idx) {
  if (nodes_[idx] == nullptr) {
    nodes_[idx] = Allocate<Node>();
    if (nodes_[idx] == nullptr) {
      return absl::ResourceExhaustedError("Out of memory");
    }
  }

  return nodes_[idx];
}

using SlabMap = SlabMapImpl<GlobalMetadataAlloc>;

}  // namespace ckmalloc
