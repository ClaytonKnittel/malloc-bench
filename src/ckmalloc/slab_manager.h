#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include "util/std_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/free_slab.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_factory.h"

namespace ckmalloc {

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
class SlabManagerImpl {
  friend class SlabManagerFixture;

 public:
  explicit SlabManagerImpl(bench::HeapFactory* heap_factory, SlabMap* slab_map,
                           size_t heap_idx);

  // Returns a pointer to the start of a slab with given `PageId`.
  void* PageStartFromId(PageId page_id) const;

  // Returns the `PageId` for the page containing `ptr`.
  PageId PageIdFromPtr(const void* ptr) const;

  // Allocates `n_pages` contiguous pages and initializes a slab metadata for
  // that region.
  template <typename S, typename... Args>
  std::optional<std::pair<PageId, S*>> Alloc(uint32_t n_pages, Args...);

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

  // The heap factory that this SlabManager allocates slabs from.
  bench::HeapFactory* heap_factory_;

  size_t heap_idx_;

  // The start of the current heap being allocated from.
  void* const heap_start_;

  // The slab manager needs to access the slab map when coalescing to know if
  // the adjacent slabs are free or allocated, and if they are free how large
  // they are.
  SlabMap* slab_map_;

  // Single-page slabs are kept in a singly-linked freelist.
  LinkedList<FreeSinglePageSlab> single_page_freelist_;

  // Multi-page slabs are kept in a red-black tree sorted by size.
  RbTree<FreeMultiPageSlab> multi_page_free_slabs_;
  // Cache a pointer to the smallest multi-page slab in the tree.
  FreeMultiPageSlab* smallest_multi_page_ = nullptr;
};

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
SlabManagerImpl<MetadataAlloc, SlabMap>::SlabManagerImpl(
    bench::HeapFactory* heap_factory, SlabMap* slab_map, size_t heap_idx)
    : heap_factory_(heap_factory),
      heap_idx_(heap_idx),
      heap_start_(heap_factory_->Instance(heap_idx)->Start()),
      slab_map_(slab_map) {}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void* SlabManagerImpl<MetadataAlloc, SlabMap>::PageStartFromId(
    PageId page_id) const {
  void* slab_start = static_cast<uint8_t*>(heap_start_) +
                     (static_cast<ptrdiff_t>(page_id.Idx()) * kPageSize);
  CK_ASSERT_GE(slab_start, heap_start_);
  CK_ASSERT_LT(slab_start, heap_factory_->Instance(heap_idx_)->End());
  return slab_start;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
PageId SlabManagerImpl<MetadataAlloc, SlabMap>::PageIdFromPtr(
    const void* ptr) const {
  CK_ASSERT_GE(ptr, heap_start_);
  CK_ASSERT_LT(ptr, heap_factory_->Instance(heap_idx_)->End());
  ptrdiff_t diff =
      static_cast<const uint8_t*>(ptr) - static_cast<uint8_t*>(heap_start_);
  return PageId(static_cast<uint32_t>(diff / kPageSize));
}

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
  slab_map_->InsertRange(page_id, page_id + n_pages - 1, initialized_slab);
  return std::make_pair(page_id, initialized_slab);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void SlabManagerImpl<MetadataAlloc, SlabMap>::Free(AllocatedSlab* slab) {
  uint32_t n_pages = slab->Pages();
  if (n_pages == 0) {
    return;
  }

  PageId start_id = slab->StartId();
  if (start_id != PageId::Zero()) {
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
  return PtrAdd<Block>(PageStartFromId(slab->StartId()),
                       Block::kFirstBlockInSlabOffset);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
size_t SlabManagerImpl<MetadataAlloc, SlabMap>::HeapSize() const {
  return static_cast<uint8_t*>(heap_factory_->Instance(heap_idx_)->End()) -
         static_cast<uint8_t*>(heap_start_);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
PageId SlabManagerImpl<MetadataAlloc, SlabMap>::HeapEndPageId() {
  ptrdiff_t diff =
      static_cast<const uint8_t*>(heap_factory_->Instance(heap_idx_)->End()) -
      static_cast<uint8_t*>(heap_start_);
  return PageId(static_cast<uint32_t>(diff / kPageSize));
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

    PageId page_id = PageIdFromPtr(slab_start);
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

  PageId page_id = PageIdFromPtr(slab_start);
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
  size_t required_size = n_pages * kPageSize;
  PageId start_id = PageId::Zero();
  // The `PageId` of where newly allocated memory willl start.
  PageId new_memory_id = HeapEndPageId();
  // We need to check that LastSlab() != nullptr here since metadata slabs do
  // not have metadata.
  if (HeapSize() != 0 && (slab = LastSlab()) != nullptr &&
      slab->Type() == SlabType::kFree) {
    FreeSlab* free_slab = slab->ToFree();
    required_size -= free_slab->Pages() * kPageSize;
    start_id = free_slab->StartId();

    // We will be taking `slab`, so remove it from its freelist.
    RemoveFreeSlab(free_slab);
  } else {
    slab = nullptr;
    start_id = new_memory_id;
  }

  void* slab_start = heap_factory_->Instance(heap_idx_)->sbrk(required_size);
  if (slab_start == nullptr) {
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

  void* slab_start = PageStartFromId(start_id);
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
  void* region_start = PageStartFromId(slab->StartId());
  if (slab->Pages() == 1) {
    RemoveSinglePageFreeSlab(
        reinterpret_cast<FreeSinglePageSlab*>(region_start));
  } else {
    RemoveMultiPageFreeSlab(reinterpret_cast<FreeMultiPageSlab*>(region_start));
  }
}

using SlabManager = SlabManagerImpl<GlobalMetadataAlloc, SlabMap>;

}  // namespace ckmalloc
