#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// The leaf size should be roughly the square root of heap size / page size.
// Round up so the leaf sizes are larger.
constexpr uint32_t kLeafShift = (kHeapSizeShift - kPageShift + 1) / 2;
// The number of pages in the leaf nodes of the slab map.
constexpr size_t kLeafSize = 1 << kLeafShift;

constexpr uint32_t kRootShift = kHeapSizeShift - kPageShift - kLeafShift;
// The length of the root node in the slab map.
constexpr size_t kRootSize = 1 << kRootShift;

class MappedSlab;

template <MetadataAllocInterface MetadataAlloc>
class SlabMapImpl {
  friend class SlabMapTest;

 public:
  // Returns the slab metadata for a given slab id. Returns `nullptr` if no
  // metadata has ever been allocated for this slab id.
  MappedSlab* FindSlab(PageId page_id) const;

  // Ensures that the slab map has allocated the necessary nodes for an entry
  // between start and end page id's. If any allocation fails, returns `false`.
  bool AllocatePath(PageId start_id, PageId end_id);

  // Inserts an association from `page_id` to `slab`.
  void Insert(PageId page_id, MappedSlab* slab);

  // Inserts an association from all pages between start_id and end_id
  // (inclusive) to `slab`.
  void InsertRange(PageId start_id, PageId end_id, MappedSlab* slab);

 private:
  template <typename T>
  class Leaf {
   public:
    T operator[](size_t idx) const {
      CK_ASSERT_LT(idx, kLeafSize);
      return vals_[idx];
    }

    void SetLeaf(size_t idx, T val) {
      vals_[idx] = val;
    }

   private:
    T vals_[kLeafSize] = {};
  };

  using SlabLeaf = Leaf<MappedSlab*>;
  using SizeLeaf = Leaf<SizeClass>;

  std::optional<int> DoAllocatePath(PageId start_id, PageId end_id);

  template <typename T>
  static T* Allocate() {
    T* ptr = static_cast<T*>(MetadataAlloc::Alloc(sizeof(T), alignof(T)));
    new (ptr) T();
    return ptr;
  }

  static size_t RootIdx(PageId page_id) {
    return page_id.Idx() / kLeafSize;
  }

  static size_t LeafIdx(PageId page_id) {
    return page_id.Idx() % kLeafSize;
  }

  SizeLeaf* SizeLeafAt(size_t idx) const {
    CK_ASSERT_LT(idx, kRootSize);
    return slab_leaves_[idx];
  }

  SlabLeaf* SlabLeafAt(size_t idx) const {
    CK_ASSERT_LT(idx, kRootSize);
    return slab_leaves_[idx];
  }

  bool TryAllocateLeaf(size_t idx);

  SizeLeaf* size_leaves_[kRootSize] = {};
  SlabLeaf* slab_leaves_[kRootSize] = {};
};

template <MetadataAllocInterface MetadataAlloc>
MappedSlab* SlabMapImpl<MetadataAlloc>::FindSlab(PageId page_id) const {
  size_t root_idx = RootIdx(page_id);
  size_t leaf_idx = LeafIdx(page_id);

  SlabLeaf* leaf = SlabLeafAt(root_idx);
  if (CK_EXPECT_FALSE(leaf == nullptr)) {
    return nullptr;
  }

  return (*leaf)[leaf_idx];
}

template <MetadataAllocInterface MetadataAlloc>
bool SlabMapImpl<MetadataAlloc>::AllocatePath(PageId start_id, PageId end_id) {
  return DoAllocatePath(start_id, end_id).has_value();
}

template <MetadataAllocInterface MetadataAlloc>
void SlabMapImpl<MetadataAlloc>::Insert(PageId page_id, MappedSlab* slab) {
  SlabLeaf& leaf = *SlabLeafAt(RootIdx(page_id));
  leaf.SetLeaf(LeafIdx(page_id), slab);
}

template <MetadataAllocInterface MetadataAlloc>
void SlabMapImpl<MetadataAlloc>::InsertRange(PageId start_id, PageId end_id,
                                             MappedSlab* slab) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  auto leaf_idxs = std::make_pair(LeafIdx(start_id), LeafIdx(end_id));

  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    SlabLeaf& leaf = *SlabLeafAt(root_idx);
    for (size_t leaf_idx = (root_idx != root_idxs.first ? 0 : leaf_idxs.first);
         leaf_idx <=
         (root_idx != root_idxs.second ? kLeafSize - 1 : leaf_idxs.second);
         leaf_idx++) {
      leaf.SetLeaf(leaf_idx, slab);
    }
  }
}

template <MetadataAllocInterface MetadataAlloc>
std::optional<int> SlabMapImpl<MetadataAlloc>::DoAllocatePath(PageId start_id,
                                                              PageId end_id) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));

  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    if (!TryAllocateLeaf(root_idx)) {
      return std::nullopt;
    }
  }

  return 0;
}

template <MetadataAllocInterface MetadataAlloc>
bool SlabMapImpl<MetadataAlloc>::TryAllocateLeaf(size_t idx) {
  if (size_leaves_[idx] == nullptr) {
    size_leaves_[idx] = Allocate<SizeLeaf>();
    slab_leaves_[idx] = Allocate<SlabLeaf>();
    if (size_leaves_[idx] == nullptr || slab_leaves_[idx] == nullptr) {
      return false;
    }
  }

  return true;
}

using SlabMap = SlabMapImpl<GlobalMetadataAlloc>;

}  // namespace ckmalloc
