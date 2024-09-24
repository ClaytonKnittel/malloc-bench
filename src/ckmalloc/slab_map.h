#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// The node size should be roughly the square root of heap size / page size.
// Round up so the leaf sizes are larger.
constexpr uint32_t kNodeShift = (kAddressBits - kPageShift + 2) / 3;
// The number of pages in the middle and leaf nodes of the slab map.
constexpr size_t kNodeSize = 1 << kNodeShift;

constexpr uint32_t kRootShift = kAddressBits - kPageShift - 2 * kNodeShift;
// The length of the root node in the slab map.
constexpr size_t kRootSize = 1 << kRootShift;

class MappedSlab;

template <MetadataAllocInterface MetadataAlloc>
class SlabMapImpl {
  friend class SlabMapTest;

 public:
  // Returns the size class for the slab spanning `page_id`. Returns
  // `SizeClass::Nil()` if the spanning slab has no size class (i.e. it's a
  // large slab). This should only be called on a page that's been allocated
  // with `AllocatePath()`.
  SizeClass FindSizeClass(PageId page_id) const;

  // Returns the slab metadata for a given slab id. Returns `nullptr` if no
  // metadata has ever been allocated for this slab id.
  MappedSlab* FindSlab(PageId page_id) const;

  // Ensures that the slab map has allocated the necessary nodes for an entry
  // between start and end page id's. If any allocation fails, returns `false`.
  bool AllocatePath(PageId start_id, PageId end_id);

  // Inserts an association from `page_id` to `slab`.
  void Insert(PageId page_id, MappedSlab* slab,
              std::optional<SizeClass> size_class = std::nullopt);

  // Inserts an association from all pages between start_id and end_id
  // (inclusive) to `slab`.
  void InsertRange(PageId start_id, PageId end_id, MappedSlab* slab,
                   std::optional<SizeClass> size_class = std::nullopt);

 private:
  template <typename T>
  class Node {
   public:
    T operator[](size_t idx) const {
      CK_ASSERT_LT(idx, kNodeSize);
      return vals_[idx];
    }

    T& operator[](size_t idx) {
      CK_ASSERT_LT(idx, kNodeSize);
      return vals_[idx];
    }

    void SetChild(size_t idx, T val) {
      vals_[idx] = val;
    }

   private:
    T vals_[kNodeSize] = {};
  };

  using SizeLeaf = Node<SizeClass>;
  using SizeNode = Node<Node<SizeClass>*>;
  using SlabLeaf = Node<MappedSlab*>;
  using SlabNode = Node<Node<MappedSlab*>*>;

  bool DoAllocatePath(PageId start_id, PageId end_id);

  template <typename T>
  static T* Allocate() {
    T* ptr = static_cast<T*>(MetadataAlloc::Alloc(sizeof(T), alignof(T)));
    if (ptr != nullptr) {
      new (ptr) T();
    }
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

  SizeNode* SizeNodeAt(size_t idx) const {
    CK_ASSERT_LT(idx, kRootSize);
    return size_nodes_[idx];
  }

  SlabNode* SlabNodeAt(size_t idx) const {
    CK_ASSERT_LT(idx, kRootSize);
    return slab_nodes_[idx];
  }

  bool TryAllocateNode(size_t idx);
  bool TryAllocateLeaf(size_t idx);

  SizeNode* size_nodes_[kRootSize] = {};
  SlabNode* slab_nodes_[kRootSize] = {};
};

template <MetadataAllocInterface MetadataAlloc>
SizeClass SlabMapImpl<MetadataAlloc>::FindSizeClass(PageId page_id) const {
  size_t root_idx = RootIdx(page_id);
  size_t middle_idx = MiddleIdx(page_id);
  size_t leaf_idx = LeafIdx(page_id);

  SizeNode* node = SizeNodeAt(root_idx);
  CK_ASSERT_NE(node, nullptr);

  SizeLeaf* leaf = (*node)[middle_idx];
  CK_ASSERT_NE(leaf, nullptr);

  return (*leaf)[leaf_idx];
}

template <MetadataAllocInterface MetadataAlloc>
MappedSlab* SlabMapImpl<MetadataAlloc>::FindSlab(PageId page_id) const {
  size_t root_idx = RootIdx(page_id);
  size_t middle_idx = MiddleIdx(page_id);
  size_t leaf_idx = LeafIdx(page_id);

  SlabNode* node = SlabNodeAt(root_idx);
  if (CK_EXPECT_FALSE(node == nullptr)) {
    return nullptr;
  }

  SlabLeaf* leaf = (*node)[middle_idx];
  if (CK_EXPECT_FALSE(leaf == nullptr)) {
    return nullptr;
  }

  return (*leaf)[leaf_idx];
}

template <MetadataAllocInterface MetadataAlloc>
bool SlabMapImpl<MetadataAlloc>::AllocatePath(PageId start_id, PageId end_id) {
  return DoAllocatePath(start_id, end_id);
}

template <MetadataAllocInterface MetadataAlloc>
void SlabMapImpl<MetadataAlloc>::Insert(PageId page_id, MappedSlab* slab,
                                        std::optional<SizeClass> size_class) {
  uint64_t root_idx = RootIdx(page_id);
  uint64_t middle_idx = MiddleIdx(page_id);
  uint64_t leaf_idx = LeafIdx(page_id);

  SlabNode& node = *SlabNodeAt(root_idx);
  SlabLeaf& leaf = *node[middle_idx];
  leaf.SetChild(leaf_idx, slab);

  if (size_class.has_value()) {
    SizeNode& node = *SizeNodeAt(root_idx);
    SizeLeaf& leaf = *node[middle_idx];
    leaf.SetChild(leaf_idx, size_class.value());
  }
}

template <MetadataAllocInterface MetadataAlloc>
void SlabMapImpl<MetadataAlloc>::InsertRange(
    PageId start_id, PageId end_id, MappedSlab* slab,
    std::optional<SizeClass> size_class) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  auto middle_idxs = std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));
  auto leaf_idxs = std::make_pair(LeafIdx(start_id), LeafIdx(end_id));

  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    SlabNode& node = *SlabNodeAt(root_idx);
    SizeNode& size_node = *SizeNodeAt(root_idx);

    for (size_t middle_idx =
             (root_idx == root_idxs.first ? middle_idxs.first : 0);
         middle_idx <=
         (root_idx == root_idxs.second ? middle_idxs.second : kNodeSize - 1);
         middle_idx++) {
      SlabLeaf& leaf = *node[middle_idx];
      SizeLeaf& size_leaf = *size_node[middle_idx];

      for (size_t leaf_idx =
               (root_idx == root_idxs.first && middle_idx == middle_idxs.first
                    ? leaf_idxs.first
                    : 0);
           leaf_idx <=
           (root_idx == root_idxs.second && middle_idx == middle_idxs.second
                ? leaf_idxs.second
                : kNodeSize - 1);
           leaf_idx++) {
        leaf.SetChild(leaf_idx, slab);
        if (size_class.has_value()) {
          size_leaf.SetChild(leaf_idx, size_class.value());
        }
      }
    }
  }
}

template <MetadataAllocInterface MetadataAlloc>
bool SlabMapImpl<MetadataAlloc>::DoAllocatePath(PageId start_id,
                                                PageId end_id) {
  auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  auto middle_idxs = std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));

  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    if (slab_nodes_[root_idx] == nullptr) {
      size_nodes_[root_idx] = Allocate<SizeNode>();
      slab_nodes_[root_idx] = Allocate<SlabNode>();
      if (size_nodes_[root_idx] == nullptr ||
          slab_nodes_[root_idx] == nullptr) {
        return false;
      }
    }

    SizeNode& size_node = *size_nodes_[root_idx];
    SlabNode& slab_node = *slab_nodes_[root_idx];
    for (size_t middle_idx =
             (root_idx == root_idxs.first ? middle_idxs.first : 0);
         middle_idx <=
         (root_idx == root_idxs.second ? middle_idxs.second : kNodeSize - 1);
         middle_idx++) {
      if (slab_node[middle_idx] == nullptr) {
        size_node[middle_idx] = Allocate<SizeLeaf>();
        slab_node[middle_idx] = Allocate<SlabLeaf>();
        if (size_node[middle_idx] == nullptr ||
            slab_node[middle_idx] == nullptr) {
          return false;
        }
      }
    }
  }

  return true;
}

template <MetadataAllocInterface MetadataAlloc>
bool SlabMapImpl<MetadataAlloc>::TryAllocateNode(size_t idx) {
  if (size_nodes_[idx] == nullptr) {
    size_nodes_[idx] = Allocate<SizeNode>();
    slab_nodes_[idx] = Allocate<SlabNode>();
    if (size_nodes_[idx] == nullptr || slab_nodes_[idx] == nullptr) {
      return false;
    }
  }

  return true;
}

template <MetadataAllocInterface MetadataAlloc>
bool SlabMapImpl<MetadataAlloc>::TryAllocateLeaf(size_t idx) {
  if (size_nodes_[idx] == nullptr) {
    size_nodes_[idx] = Allocate<SizeNode>();
    slab_nodes_[idx] = Allocate<SlabNode>();
    if (size_nodes_[idx] == nullptr || slab_nodes_[idx] == nullptr) {
      return false;
    }
  }

  return true;
}

using SlabMap = SlabMapImpl<GlobalMetadataAlloc>;

}  // namespace ckmalloc
