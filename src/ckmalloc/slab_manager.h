#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "util/std_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/free_slab.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
class SlabManagerImpl {
  friend class SlabManagerFixture;
  friend class GlobalState;
  friend class TraceReplayer;

 public:
  explicit SlabManagerImpl(bench::Heap* heap, SlabMap* slab_map);

  // Allocates `n_pages` contiguous pages and initializes a slab metadata for
  // that region.
  template <typename S, typename... Args>
  std::optional<std::pair<PageId, S*>> Alloc(uint32_t n_pages, Args...);

  // Attempts to resize the slab in-place, extending it or shrinking it without
  // changing the start id of the slab. If this returns true, the resize was
  // successful, and the slab size metadata will have been updated. If this
  // returns false, then no modifications will have been made.
  bool Resize(AllocatedSlab* slab, uint32_t new_size);

  // Frees the slab and takes ownership of the `Slab` metadata object.
  void Free(AllocatedSlab* slab);

  // Returns the first block in this slab, i.e. the block with lowest starting
  // address.
  Block* FirstBlockInBlockedSlab(const BlockedSlab* slab) const;

 private:
  size_t HeapSize() const;

  // Returns the `PageId` of the end of the heap (i.e. one page past the last
  // allocated slab).
  PageId HeapEndPageId();

  // Returns the slab metadata for the rightmost slab, i.e. the slab with
  // highest start `PageId`. Should only be called if the heap is not empty.
  MappedSlab* LastSlab();

  // Allocates `n_pages` contiguous pages, returning the `PageId` of the first
  // page in the slab, and potentially returning an allocated a `Slab` metadata
  // without initializing it. If there was no availability, it returns
  // `nullopt`. This method will not attempt to allocate slab metadata, so if
  // there was no extra slab metadata relinquished by changes made to the heap,
  // the slab metadata pointer returned will be null and the user has to
  // allocate slab metadata.
  std::optional<std::pair<PageId, Slab*>> Alloc(uint32_t n_pages);

  // Finds a region of memory to return for `Alloc`, returning the `PageId` of
  // the beginning of the region and a `Slab` metadata object that may be used
  // to hold metadata for this region. This method will not increase the size of
  // the heap, and may return `std::nullopt` if there was no memory region large
  // enough for this allocation already available.
  std::optional<std::pair<PageId, MappedSlab*>> DoAllocWithoutSbrk(
      uint32_t n_pages);

  // Tries to allocate `n_pages` at the end of the heap, which should increase
  // the size of the heap. This should be called if allocating within the heap
  // has already failed. The slab map will allocate the necessary nodes for map
  // entries for the new slab.
  std::optional<std::pair<PageId, Slab*>> AllocEndWithSbrk(uint32_t n_pages);

  // Inserts a single-page free slab into the slab freelist.
  void InsertSinglePageFreeSlab(FreeSinglePageSlab* slab_start);

  // Inserts a multi-page free slab into the freelist.
  void InsertMultiPageFreeSlab(FreeMultiPageSlab* slab_start, uint32_t n_pages);

  // Given a `Slab` metadata object to use, a start `PageId`, and number of
  // pages, initializes the `Slab` metadata to describe this region as free, and
  // inserts it into the necessary data structures to track this free region.
  // This does not coalesce with neighbors.
  void FreeRegion(Slab* slab, PageId start_id, uint32_t n_pages);

  // Removes a single-page free slab from the slab freelist, allowing it to be
  // allocated or merged into another slab.
  void RemoveSinglePageFreeSlab(FreeSinglePageSlab* slab_start);

  // Removes a multi-page free slab from the freelist, allowing it to be
  // allocated or merged into another slab.
  void RemoveMultiPageFreeSlab(FreeMultiPageSlab* slab_start);

  // Removes a free slab with given metadata from the freelist it is in,
  // allowing it to be allocated or merged into another slab.
  void RemoveFreeSlab(FreeSlab* slab);

  // Extends the heap by `n_pages`, returning true if the operation was
  // successful.
  //
  // `heap_end` is only passed since callers typically already have this value.
  // This is a performance consideration, and `heap_end` should always equal
  // `HeapEndPageId()`.
  bool ExtendHeap(PageId heap_end, uint32_t n_pages);

  // The heap that this SlabManager allocates slabs from.
  bench::Heap* heap_;

  // The start of the current heap being allocated from.
  void* const heap_start_;

  // The slab manager needs to access the slab map when coalescing to know if
  // the adjacent slabs are free or allocated, and if they are free how large
  // they are.
  SlabMap* const slab_map_;

  // Single-page slabs are kept in a singly-linked freelist.
  LinkedList<FreeSinglePageSlab> single_page_freelist_;

  // Multi-page slabs are kept in a red-black tree sorted by size.
  RbTree<FreeMultiPageSlab> multi_page_free_slabs_;
  // Cache a pointer to the smallest multi-page slab in the tree.
  FreeMultiPageSlab* smallest_multi_page_ = nullptr;
};

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
SlabManagerImpl<MetadataAlloc, SlabMap>::SlabManagerImpl(bench::Heap* heap,
                                                         SlabMap* slab_map)
    : heap_(heap), heap_start_(heap_->Start()), slab_map_(slab_map) {}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
template <typename S, typename... Args>
std::optional<std::pair<PageId, S*>>
SlabManagerImpl<MetadataAlloc, SlabMap>::Alloc(uint32_t n_pages, Args... args) {
  static_assert(kHasMetadata<S>,
                "You may only directly allocate non-metadata slabs.");
  using AllocResult = std::pair<PageId, Slab*>;

  DEFINE_OR_RETURN_OPT(AllocResult, result, Alloc(n_pages));
  auto [page_id, slab] = std::move(result);
  if (slab == nullptr) {
    slab = MetadataAlloc::SlabAlloc();
    if (slab == nullptr) {
      // TODO: clean up uninitialized allocated memory.
      return std::nullopt;
    }
  }

  S* initialized_slab =
      slab->Init<S>(page_id, n_pages, std::forward<Args>(args)...);

  SizeClass size_class;
  if constexpr (HasSizeClassT<S>) {
    size_class = initialized_slab->SizeClass();
  } else {
    size_class = SizeClass::Nil();
  }

  if constexpr (kHasOneAllocation<S>) {
    slab_map_->Insert(page_id, initialized_slab, size_class);
    slab_map_->Insert(page_id + n_pages - 1, initialized_slab, size_class);
  } else {
    slab_map_->InsertRange(page_id, page_id + n_pages - 1, initialized_slab,
                           size_class);
  }
  return std::make_pair(page_id, initialized_slab);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
bool SlabManagerImpl<MetadataAlloc, SlabMap>::Resize(AllocatedSlab* slab,
                                                     uint32_t new_size) {
  CK_ASSERT_NE(new_size, 0);
  CK_ASSERT_FALSE(slab->HasSizeClass());

  uint32_t n_pages = slab->Pages();
  if (new_size == n_pages) {
    return true;
  }

  const PageId slab_start = slab->StartId();
  MappedSlab* next_slab = slab_map_->FindSlab(slab->EndId() + 1);
  // The start of the next adjacent free slab, if there will be one.
  const PageId free_start = slab_start + new_size;
  // This will be assigned the size of the next adjacent free slab.
  uint32_t free_size;
  // This will be available slab metadata that can be used for the new/modified
  // next adjacent free slab.
  Slab* free_meta;

  if (new_size < n_pages) {
    free_size = n_pages - new_size;

    // Check the next slab, if it's free then we can extend it backwards.
    if (next_slab != nullptr && next_slab->Type() == SlabType::kFree) {
      FreeSlab* next_free_slab = next_slab->ToFree();
      free_meta = next_free_slab;
      free_size += next_free_slab->Pages();

      RemoveFreeSlab(next_free_slab);
    } else {
      // Otherwise we need to make a new free region.
      free_meta = MetadataAlloc::SlabAlloc();
      if (free_meta == nullptr) {
        return false;
      }
    }

    // If this is a single-allocation slab, the end of this slab may not
    // correctly map to this slab.
    if (HasOneAllocation(slab->Type())) {
      slab_map_->Insert(free_start - 1, slab, SizeClass::Nil());
    }
  } else {
    FreeSlab* next_free_slab;
    uint32_t available_pages = n_pages;

    if (next_slab == nullptr) {
      next_free_slab = nullptr;
    } else if (next_slab->Type() != SlabType::kFree) {
      // If the next slab isn't free, we can't extend this slab.
      return false;
    } else {
      next_free_slab = next_slab->ToFree();
      available_pages += next_free_slab->Pages();
    }

    // Check that the next free slab is large enough to accomodate the size
    // increase.
    if (available_pages < new_size) {
      // If not, check if `next_free_slab` is at the end of the heap. If so, we
      // can attempt to extend the heap.
      PageId next_next_start = slab_start + available_pages;
      MappedSlab* next_next_slab = slab_map_->FindSlab(next_next_start);
      if (next_next_slab != nullptr) {
        // If the next slab isn't null, that means there is some slab after this
        // one. We don't have enough space to extend this slab.
        return false;
      }

      // Try extending the heap the extra required pages.
      if (!ExtendHeap(next_next_start, new_size - available_pages)) {
        return false;
      }

      // If we successfully extended the heap, then we have exactly enough space
      // for this extension.
      available_pages = new_size;
    }

    // Remove the next free slab from the freelist, since we'll be changing its
    // size.
    if (next_free_slab != nullptr) {
      RemoveFreeSlab(next_free_slab);
    }

    // At this point, we know for certain that the slab can be extended and will
    // not fail. Update the slab map for the soon-to-be-added pages.
    if (HasOneAllocation(slab->Type())) {
      slab_map_->Insert(free_start - 1, slab, SizeClass::Nil());
    } else {
      slab_map_->InsertRange(slab_start + n_pages, free_start - 1, slab,
                             SizeClass::Nil());
    }

    if (available_pages == new_size) {
      // If we exactly fill the next free slab, then we can just change our size
      // and free the old free slab metadata.
      slab->SetSize(new_size);
      if (next_free_slab != nullptr) {
        MetadataAlloc::SlabFree(next_free_slab);
      }
      return true;
    }

    // If we don't exactly fill the next free slab, we need to add a new free
    // slab after the end of the newly extended slab.
    free_size = available_pages - new_size;
    free_meta = next_free_slab;
  }

  if (free_meta != nullptr) {
    FreeRegion(free_meta, free_start, free_size);
  }
  slab->SetSize(new_size);
  return true;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void SlabManagerImpl<MetadataAlloc, SlabMap>::Free(AllocatedSlab* slab) {
  uint32_t n_pages = slab->Pages();
  if (n_pages == 0) {
    return;
  }

  PageId start_id = slab->StartId();
  {
    MappedSlab* prev_slab = slab_map_->FindSlab(start_id - 1);
    if (prev_slab != nullptr && prev_slab->Type() == SlabType::kFree) {
      FreeSlab* prev_free_slab = prev_slab->ToFree();
      start_id = prev_free_slab->StartId();
      n_pages += prev_free_slab->Pages();
      RemoveFreeSlab(prev_free_slab);
      MetadataAlloc::SlabFree(prev_free_slab);
    }
  }

  {
    MappedSlab* next_slab = slab_map_->FindSlab(slab->EndId() + 1);
    if (next_slab != nullptr && next_slab->Type() == SlabType::kFree) {
      FreeSlab* next_free_slab = next_slab->ToFree();
      n_pages += next_free_slab->Pages();
      RemoveFreeSlab(next_free_slab);
      MetadataAlloc::SlabFree(next_free_slab);
    }
  }

  FreeRegion(slab, start_id, n_pages);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
Block* SlabManagerImpl<MetadataAlloc, SlabMap>::FirstBlockInBlockedSlab(
    const BlockedSlab* slab) const {
  return BlockedSlab::FirstBlock(slab->StartId().PageStart());
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
size_t SlabManagerImpl<MetadataAlloc, SlabMap>::HeapSize() const {
  return static_cast<size_t>(static_cast<uint8_t*>(heap_->End()) -
                             static_cast<uint8_t*>(heap_start_));
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
PageId SlabManagerImpl<MetadataAlloc, SlabMap>::HeapEndPageId() {
  return PageId::FromPtr(heap_->End());
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
MappedSlab* SlabManagerImpl<MetadataAlloc, SlabMap>::LastSlab() {
  CK_ASSERT_NE(HeapSize(), 0);
  return slab_map_->FindSlab(HeapEndPageId() - 1);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
std::optional<std::pair<PageId, Slab*>>
SlabManagerImpl<MetadataAlloc, SlabMap>::Alloc(uint32_t n_pages) {
  return OptionalOrElse<std::pair<PageId, Slab*>>(
      DoAllocWithoutSbrk(n_pages),
      [this, n_pages]() { return AllocEndWithSbrk(n_pages); });
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
std::optional<std::pair<PageId, MappedSlab*>>
SlabManagerImpl<MetadataAlloc, SlabMap>::DoAllocWithoutSbrk(uint32_t n_pages) {
  if (n_pages == 1 && !single_page_freelist_.Empty()) {
    FreeSinglePageSlab* slab_start = single_page_freelist_.Front();
    RemoveSinglePageFreeSlab(slab_start);

    PageId page_id = PageId::FromPtr(slab_start);
    MappedSlab* slab = slab_map_->FindSlab(page_id);
    return std::make_pair(page_id, slab);
  }
  if (smallest_multi_page_ == nullptr) {
    return std::nullopt;
  }

  FreeMultiPageSlab* slab_start;
  if (n_pages <= 2) {
    slab_start = smallest_multi_page_;
  } else {
    slab_start = multi_page_free_slabs_.LowerBound(
        [n_pages](const FreeMultiPageSlab& slab) {
          return slab.Pages() >= n_pages;
        });
  }
  if (slab_start == nullptr) {
    // No slabs large enough were found.
    return std::nullopt;
  }

  RemoveMultiPageFreeSlab(slab_start);

  PageId page_id = PageId::FromPtr(slab_start);
  MappedSlab* slab = slab_map_->FindSlab(page_id);
  CK_ASSERT_NE(slab, nullptr);
  uint32_t actual_pages = slab_start->Pages();
  CK_ASSERT_GE(actual_pages, n_pages);
  if (actual_pages != n_pages) {
    // This region was already free, so we know the next adjacent slab cannot be
    // free, and we are about to allocate the region before it, so we never need
    // to coalesce here.
    FreeRegion(slab, page_id + n_pages, actual_pages - n_pages);
    // We have used the slab metadata for this new free region, so we will need
    // to allocate our own later.
    slab = nullptr;
  }

  return std::make_pair(page_id, slab);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
std::optional<std::pair<PageId, Slab*>>
SlabManagerImpl<MetadataAlloc, SlabMap>::AllocEndWithSbrk(uint32_t n_pages) {
  // If we have allocated anything, check if the last slab is free. If so, we
  // can use it and only allocate the difference past the end of the heap.
  Slab* slab;
  uint32_t required_pages = n_pages;
  PageId start_id = PageId::Nil();
  // The `PageId` of where newly allocated memory willl start.
  PageId new_memory_id = HeapEndPageId();
  // We need to check that LastSlab() != nullptr here since metadata slabs do
  // not have metadata.
  if (HeapSize() != 0 && (slab = LastSlab()) != nullptr &&
      slab->Type() == SlabType::kFree) {
    FreeSlab* free_slab = slab->ToFree();
    required_pages -= free_slab->Pages();
    start_id = free_slab->StartId();

    // We will be taking `slab`, so remove it from its freelist.
    RemoveFreeSlab(free_slab);
  } else {
    slab = nullptr;
    start_id = new_memory_id;
  }

  if (!ExtendHeap(new_memory_id, required_pages)) {
    return std::nullopt;
  }

  if (!slab_map_->AllocatePath(new_memory_id, start_id + n_pages - 1)) {
    return std::nullopt;
  }

  return std::make_pair(start_id, slab);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void SlabManagerImpl<MetadataAlloc, SlabMap>::InsertSinglePageFreeSlab(
    FreeSinglePageSlab* slab_start) {
  single_page_freelist_.InsertFront(slab_start);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void SlabManagerImpl<MetadataAlloc, SlabMap>::InsertMultiPageFreeSlab(
    FreeMultiPageSlab* slab_start, uint32_t n_pages) {
  auto* slab = new (slab_start) FreeMultiPageSlab(n_pages);
  multi_page_free_slabs_.Insert(slab);
  // Rb biases toward inserting to the right, so if slab == smallest already,
  // it will certainly not be inserted before it in the tree.
  if (smallest_multi_page_ == nullptr || *slab < *smallest_multi_page_) {
    smallest_multi_page_ = slab;
  }
  CK_ASSERT_EQ(multi_page_free_slabs_.Prev(smallest_multi_page_), nullptr);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void SlabManagerImpl<MetadataAlloc, SlabMap>::FreeRegion(Slab* slab,
                                                         PageId start_id,
                                                         uint32_t n_pages) {
  PageId end_id = start_id + n_pages - 1;

  FreeSlab* free_slab = slab->Init<FreeSlab>(start_id, n_pages);
  // We only need to map this slab to the first and last page of the slab, since
  // those will be the only pages queried from this method, and no
  // user-allocated data lives within a free slab.
  slab_map_->Insert(start_id, free_slab);
  slab_map_->Insert(end_id, free_slab);

  void* slab_start = start_id.PageStart();
  if (n_pages == 1) {
    auto* slab = new (slab_start) FreeSinglePageSlab();
    InsertSinglePageFreeSlab(slab);
  } else {
    auto* slab = new (slab_start) FreeMultiPageSlab(n_pages);
    InsertMultiPageFreeSlab(slab, n_pages);
  }
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void SlabManagerImpl<MetadataAlloc, SlabMap>::RemoveSinglePageFreeSlab(
    FreeSinglePageSlab* slab_start) {
  single_page_freelist_.Remove(slab_start);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void SlabManagerImpl<MetadataAlloc, SlabMap>::RemoveMultiPageFreeSlab(
    FreeMultiPageSlab* slab_start) {
  if (slab_start == smallest_multi_page_) {
    smallest_multi_page_ = static_cast<FreeMultiPageSlab*>(
        multi_page_free_slabs_.Next(slab_start));
  }
  multi_page_free_slabs_.Remove(slab_start);
  CK_ASSERT_TRUE(smallest_multi_page_ == nullptr ||
                 multi_page_free_slabs_.Prev(smallest_multi_page_) == nullptr);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void SlabManagerImpl<MetadataAlloc, SlabMap>::RemoveFreeSlab(FreeSlab* slab) {
  void* region_start = slab->StartId().PageStart();
  if (slab->Pages() == 1) {
    RemoveSinglePageFreeSlab(
        reinterpret_cast<FreeSinglePageSlab*>(region_start));
  } else {
    RemoveMultiPageFreeSlab(reinterpret_cast<FreeMultiPageSlab*>(region_start));
  }
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
bool SlabManagerImpl<MetadataAlloc, SlabMap>::ExtendHeap(PageId heap_end,
                                                         uint32_t n_pages) {
  CK_ASSERT_EQ(heap_end, HeapEndPageId());
  void* slab_start = heap_->sbrk(n_pages * kPageSize);
  if (slab_start == nullptr) {
    return false;
  }

  if (!slab_map_->AllocatePath(heap_end, heap_end + n_pages - 1)) {
    return false;
  }

  return true;
}

using SlabManager = SlabManagerImpl<GlobalMetadataAlloc, SlabMap>;

}  // namespace ckmalloc
