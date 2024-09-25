#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

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
  friend class SlabManagerFixture;

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

  // Deallocates the mappings within the given range of pages for reuse by
  // future `AllocatePath()` calls. They all must be allocated.
  void DeallocatePath(PageId start_id, PageId end_id);

  // Inserts an association from `page_id` to `slab`.
  void Insert(PageId page_id, MappedSlab* slab,
              std::optional<SizeClass> size_class = std::nullopt);

  // Inserts an association from all pages between start_id and end_id
  // (inclusive) to `slab`.
  void InsertRange(PageId start_id, PageId end_id, MappedSlab* slab,
                   std::optional<SizeClass> size_class = std::nullopt);

 private:
  class Leaf {
    friend class SlabManagerFixture;

   public:
    // If true, this node's children are all deallocated and the node may be
    // freed.
    bool Empty() const {
      return allocated_count_ == 0;
    }

    MappedSlab* GetSlab(size_t idx) {
      CK_ASSERT_LT(idx, kNodeSize);
      return slabs_[idx];
    }

    SizeClass GetSizeClass(size_t idx) {
      CK_ASSERT_LT(idx, kNodeSize);
      return size_classes_[idx];
    }

    void SetChild(size_t idx, MappedSlab* slab,
                  std::optional<SizeClass> size_class) {
      CK_ASSERT_LT(idx, kNodeSize);
      if (size_class.has_value()) {
        size_classes_[idx] = size_class.value();
      }
      slabs_[idx] = slab;
    }

    void ClearChild(size_t idx) {
      CK_ASSERT_LT(idx, kNodeSize);
      // Only clear the child in debug builds, since we have assertions that
      // check that children are set in slab map lookup. But really, we should
      // expect no part of the code to ever try looking up a deallocated page.
#ifndef NDEBUG
      slabs_[idx] = nullptr;
      size_classes_[idx] = SizeClass::Nil();
#endif
    }

    void AddAllocatedCount(uint32_t count) {
      CK_ASSERT_LE(count + allocated_count_, kNodeSize);
      allocated_count_ += count;
    }

    void RemoveAllocatedCount(uint32_t count) {
      CK_ASSERT_LE(count, allocated_count_);
      allocated_count_ -= count;
    }

   private:
    SizeClass size_classes_[kNodeSize] = {};
    MappedSlab* slabs_[kNodeSize] = {};
    uint32_t allocated_count_ = 0;
  };

  class Node {
    friend class SlabManagerFixture;

   public:
    // If true, this node's children are all deallocated and the node may be
    // freed.
    bool Empty() const {
      return allocated_count_ == 0;
    }

    Leaf* GetLeaf(size_t idx) {
      CK_ASSERT_LT(idx, kNodeSize);
      return leaves_[idx];
    }

    void SetChild(size_t idx, Leaf* leaf) {
      CK_ASSERT_LT(idx, kNodeSize);
      leaves_[idx] = leaf;
      allocated_count_++;
    }

    void ClearChild(size_t idx) {
      CK_ASSERT_LT(idx, kNodeSize);
      // Only clear the child in debug builds, since we have assertions that
      // check that children are set in slab map lookup. But really, we should
      // expect no part of the code to ever try looking up a deallocated page.
#ifndef NDEBUG
      leaves_[idx] = nullptr;
#endif
      allocated_count_--;
    }

   private:
    Leaf* leaves_[kNodeSize] = {};
    uint32_t allocated_count_ = 0;
  };

  template <size_t Size>
  class FreeNode {
   public:
    FreeNode* Next() {
      return next_;
    }

    template <typename N,
              typename = typename std::enable_if_t<Size == sizeof(N)>>
    static FreeNode* SetNext(N* node, FreeNode* next) {
      FreeNode* free_node = reinterpret_cast<FreeNode*>(node);
      free_node->next_ = next;
      return free_node;
    }

    template <typename N,
              typename = typename std::enable_if_t<Size == sizeof(N)>>
    N* To() {
      return reinterpret_cast<N*>(this);
    }

   private:
    union {
      FreeNode* next_;
      uint8_t data_[Size];
    };
  };

  template <typename T>
  T* Allocate();

  template <typename T>
  void Free(T* node);

  static size_t RootIdx(PageId page_id) {
    return page_id.Idx() / (kNodeSize * kNodeSize);
  }

  static size_t MiddleIdx(PageId page_id) {
    return (page_id.Idx() / kNodeSize) % kNodeSize;
  }

  static size_t LeafIdx(PageId page_id) {
    return page_id.Idx() % kNodeSize;
  }

  Node* NodeAt(size_t idx) const {
    CK_ASSERT_LT(idx, kRootSize);
    return nodes_[idx];
  }

  Node* nodes_[kRootSize] = {};

  // The following are freelists of slab map nodes, for reuse by future
  // allocations.
  static constexpr size_t kFreeNodesSize = sizeof(Node);
  static constexpr size_t kFreeLeavesSize = sizeof(Leaf);

  FreeNode<kFreeNodesSize>* free_nodes_ = nullptr;
  FreeNode<kFreeLeavesSize>* free_leaves_ = nullptr;
};

template <MetadataAllocInterface MetadataAlloc>
SizeClass SlabMapImpl<MetadataAlloc>::FindSizeClass(PageId page_id) const {
  size_t root_idx = RootIdx(page_id);
  size_t middle_idx = MiddleIdx(page_id);
  size_t leaf_idx = LeafIdx(page_id);

  Node* node = NodeAt(root_idx);
  CK_ASSERT_NE(node, nullptr);

  Leaf* leaf = node->GetLeaf(middle_idx);
  CK_ASSERT_NE(leaf, nullptr);

  return leaf->GetSizeClass(leaf_idx);
}

template <MetadataAllocInterface MetadataAlloc>
MappedSlab* SlabMapImpl<MetadataAlloc>::FindSlab(PageId page_id) const {
  size_t root_idx = RootIdx(page_id);
  size_t middle_idx = MiddleIdx(page_id);
  size_t leaf_idx = LeafIdx(page_id);

  Node* node = NodeAt(root_idx);
  if (CK_EXPECT_FALSE(node == nullptr)) {
    return nullptr;
  }

  Leaf* leaf = node->GetLeaf(middle_idx);
  if (CK_EXPECT_FALSE(leaf == nullptr)) {
    return nullptr;
  }

  return leaf->GetSlab(leaf_idx);
}

template <MetadataAllocInterface MetadataAlloc>
bool SlabMapImpl<MetadataAlloc>::AllocatePath(PageId start_id, PageId end_id) {
  // TODO: Do this with iterators.
  const auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  const auto middle_idxs =
      std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));
  const auto leaf_idxs = std::make_pair(LeafIdx(start_id), LeafIdx(end_id));

  CK_ASSERT_LE(root_idxs.first, root_idxs.second);
  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    const bool first_iter = root_idx == root_idxs.first;
    const bool last_iter = root_idx == root_idxs.second;

    if (nodes_[root_idx] == nullptr) {
      nodes_[root_idx] = Allocate<Node>();
      if (nodes_[root_idx] == nullptr) {
        // TODO: Free all of the nodes that successfully allocated.
        return false;
      }
    }

    const size_t middle_idx_low = (first_iter ? middle_idxs.first : 0);
    const size_t middle_idx_high =
        (last_iter ? middle_idxs.second : kNodeSize - 1);
    CK_ASSERT_LE(middle_idx_low, middle_idx_high);

    Node& node = *nodes_[root_idx];
    for (size_t middle_idx = middle_idx_low; middle_idx <= middle_idx_high;
         middle_idx++) {
      const bool first_inner_iter =
          first_iter && middle_idx == middle_idxs.first;
      const bool last_inner_iter =
          last_iter && middle_idx == middle_idxs.second;

      Leaf* leaf;
      if (node.GetLeaf(middle_idx) == nullptr) {
        leaf = Allocate<Leaf>();
        if (leaf == nullptr) {
          // TODO: Free all of the nodes that successfully allocated.
          return false;
        }
        node.SetChild(middle_idx, leaf);
      } else {
        leaf = node.GetLeaf(middle_idx);
      }

      uint32_t new_leaves =
          1 + (last_inner_iter ? leaf_idxs.second : kNodeSize - 1) -
          (first_inner_iter ? leaf_idxs.first : 0);
      leaf->AddAllocatedCount(new_leaves);
    }
  }

  return true;
}

template <MetadataAllocInterface MetadataAlloc>
void SlabMapImpl<MetadataAlloc>::DeallocatePath(PageId start_id,
                                                PageId end_id) {
  const auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  const auto middle_idxs =
      std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));
  const auto leaf_idxs = std::make_pair(LeafIdx(start_id), LeafIdx(end_id));

  CK_ASSERT_LE(root_idxs.first, root_idxs.second);
  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    const bool first_iter = root_idx == root_idxs.first;
    const bool last_iter = root_idx == root_idxs.second;

    Node* node = NodeAt(root_idx);
    CK_ASSERT_NE(node, nullptr);

    const size_t middle_idx_low = (first_iter ? middle_idxs.first : 0);
    const size_t middle_idx_high =
        (last_iter ? middle_idxs.second : kNodeSize - 1);
    CK_ASSERT_LE(middle_idx_low, middle_idx_high);

    for (size_t middle_idx = middle_idx_low; middle_idx <= middle_idx_high;
         middle_idx++) {
      const bool first_inner_iter =
          first_iter && middle_idx == middle_idxs.first;
      const bool last_inner_iter =
          last_iter && middle_idx == middle_idxs.second;

      Leaf* leaf = node->GetLeaf(middle_idx);
      CK_ASSERT_NE(leaf, nullptr);

      // This loop will not happen in production builds with NDEBUG=1.
      for (uint32_t i = (first_inner_iter ? leaf_idxs.first : 0);
           i <= (last_inner_iter ? leaf_idxs.second : kNodeSize - 1); i++) {
        leaf->ClearChild(i);
      }

      uint32_t removed_leaves =
          1 + (last_inner_iter ? leaf_idxs.second : kNodeSize - 1) -
          (first_inner_iter ? leaf_idxs.first : 0);
      leaf->RemoveAllocatedCount(removed_leaves);

      if (leaf->Empty()) {
        node->ClearChild(middle_idx);
        Free(leaf);
      }
    }

    if (node->Empty()) {
#ifndef NDEBUG
      nodes_[root_idx] = nullptr;
#endif
      Free(node);
    }
  }
}

template <MetadataAllocInterface MetadataAlloc>
void SlabMapImpl<MetadataAlloc>::Insert(PageId page_id, MappedSlab* slab,
                                        std::optional<SizeClass> size_class) {
  uint64_t root_idx = RootIdx(page_id);
  uint64_t middle_idx = MiddleIdx(page_id);
  uint64_t leaf_idx = LeafIdx(page_id);

  Node& node = *NodeAt(root_idx);
  Leaf& leaf = *node.GetLeaf(middle_idx);
  leaf.SetChild(leaf_idx, slab, size_class);
}

template <MetadataAllocInterface MetadataAlloc>
void SlabMapImpl<MetadataAlloc>::InsertRange(
    PageId start_id, PageId end_id, MappedSlab* slab,
    std::optional<SizeClass> size_class) {
  const auto root_idxs = std::make_pair(RootIdx(start_id), RootIdx(end_id));
  const auto middle_idxs =
      std::make_pair(MiddleIdx(start_id), MiddleIdx(end_id));
  const auto leaf_idxs = std::make_pair(LeafIdx(start_id), LeafIdx(end_id));

  CK_ASSERT_LE(root_idxs.first, root_idxs.second);
  for (size_t root_idx = root_idxs.first; root_idx <= root_idxs.second;
       root_idx++) {
    const bool first_iter = root_idx == root_idxs.first;
    const bool last_iter = root_idx == root_idxs.second;

    Node& node = *NodeAt(root_idx);

    const size_t middle_idx_low = (first_iter ? middle_idxs.first : 0);
    const size_t middle_idx_high =
        (last_iter ? middle_idxs.second : kNodeSize - 1);
    CK_ASSERT_LE(middle_idx_low, middle_idx_high);

    for (size_t middle_idx = middle_idx_low; middle_idx <= middle_idx_high;
         middle_idx++) {
      Leaf& leaf = *node.GetLeaf(middle_idx);

      const bool first_inner_iter =
          first_iter && middle_idx == middle_idxs.first;
      const bool last_inner_iter =
          last_iter && middle_idx == middle_idxs.second;
      const size_t leaf_idx_low = (first_inner_iter ? leaf_idxs.first : 0);
      const size_t leaf_idx_high =
          (last_inner_iter ? leaf_idxs.second : kNodeSize - 1);
      CK_ASSERT_LE(leaf_idx_low, leaf_idx_high);

      for (size_t leaf_idx = leaf_idx_low; leaf_idx <= leaf_idx_high;
           leaf_idx++) {
        leaf.SetChild(leaf_idx, slab, size_class);
      }
    }
  }
}

template <MetadataAllocInterface MetadataAlloc>
template <typename T>
T* SlabMapImpl<MetadataAlloc>::Allocate() {
  T* ptr;
  FreeNode<sizeof(T)>** list_head;
  if constexpr (sizeof(T) == kFreeNodesSize) {
    list_head = &free_nodes_;
  } else if constexpr (sizeof(T) == kFreeLeavesSize) {
    list_head = &free_leaves_;
  } else {
    static_assert(false);
  }
  if (*list_head != nullptr) {
    ptr = (*list_head)->template To<T>();
    *list_head = (*list_head)->Next();
  } else {
    ptr = static_cast<T*>(MetadataAlloc::Alloc(sizeof(T), alignof(T)));
  }

  if (ptr != nullptr) {
    new (ptr) T();
  }
  return ptr;
}

template <MetadataAllocInterface MetadataAlloc>
template <typename T>
void SlabMapImpl<MetadataAlloc>::Free(T* node) {
  FreeNode<sizeof(T)>** list_head;
  if constexpr (sizeof(T) == kFreeNodesSize) {
    list_head = &free_nodes_;
  } else if constexpr (sizeof(T) == kFreeLeavesSize) {
    list_head = &free_leaves_;
  } else {
    static_assert(false);
  }

  auto* free_node = FreeNode<sizeof(T)>::SetNext(node, *list_head);
  *list_head = free_node;
}

using SlabMap = SlabMapImpl<GlobalMetadataAlloc>;

}  // namespace ckmalloc
