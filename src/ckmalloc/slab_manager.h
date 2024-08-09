#pragma once

#include <cstdint>
#include <optional>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/free_slab.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

using SlabRbTree = RbTree<FreeMultiPageSlab>;

template <MetadataAllocInterface MetadataAlloc>
class SlabManagerImpl {
  friend class SlabManagerTest;

 public:
  explicit SlabManagerImpl(bench::Heap* heap,
                           SlabMapImpl<MetadataAlloc>* slab_map);

  // Returns a pointer to the start of a slab with given `PageId`.
  void* PageStartFromId(PageId page_id) const;

  // Returns the `PageId` for the page containing `ptr`.
  PageId PageIdFromPtr(const void* ptr) const;

  // Allocates `n_pages` contiguous pages, returning the `PageId` of the first
  // page in the slab, and an allocated a `Slab` metadata and initializing it
  // based on `slab_type` for this range of pages, if there was availability,
  // otherwise returning `nullopt`. If something was allocated, the SlabMap will
  // have been updated to map all pages within the alloc to the returned Slab
  // metadata.
  //
  // TODO: some slab types may take additional construction args, will need to
  // pass those in a variadic way here.
  std::optional<std::pair<PageId, Slab*>> Alloc(uint32_t n_pages,
                                                SlabType slab_type);

  // Frees the slab and takes ownership of the `Slab` metadata object.
  void Free(Slab* slab);

 private:
  // Finds a region of memory to return for `Alloc`, returning the `PageId` of
  // the beginning of the region and a `Slab` metadata object that may be used
  // to hold metadata for this region. This method will not increase the size of
  // the heap, and may return `std::nullopt` if there was no memory region large
  // enough for this allocation already available.
  std::optional<std::pair<PageId, Slab*>> DoAllocWithoutSbrk(uint32_t n_pages);

  // Removes a single-page free slab from the slab freelist, allowing it to be
  // allocated or merged into another slab.
  void RemoveSinglePageFreeSlab(FreeSinglePageSlab* slab_start);

  // Removes a multi-page free slab from the freelist, allowing it to be
  // allocated or merged into another slab.
  void RemoveMultiPageFreeSlab(FreeMultiPageSlab* slab_start);

  // Removes a free slab with given metadata from the freelist it is in,
  // allowing it to be allocated or merged into another slab.
  void RemoveFreeSlab(Slab* slab);

  // The heap that this SlabManager allocates slabs from.
  bench::Heap* heap_;
  // Cache the heap start from `heap_`, which is guaranteed to never change.
  void* const heap_start_;

  // The slab manager needs to access the slab map when coalescing to know if
  // the adjacent slabs are free or allocated, and if they are free how large
  // they are.
  SlabMapImpl<MetadataAlloc>* slab_map_;

  // Single-page slabs are kept in a singly-linked freelist.
  LinkedList<FreeSinglePageSlab> single_page_freelist_;

  // Multi-page slabs are kept in a red-black tree sorted by size.
  SlabRbTree multi_page_free_slabs_;
  // Cache a pointer to the smallest multi-page slab in the tree.
  FreeMultiPageSlab* smallest_multi_page_ = nullptr;
};

template <MetadataAllocInterface MetadataAlloc>
SlabManagerImpl<MetadataAlloc>::SlabManagerImpl(
    bench::Heap* heap, SlabMapImpl<MetadataAlloc>* slab_map)
    : heap_(heap), heap_start_(heap->Start()), slab_map_(slab_map) {}

template <MetadataAllocInterface MetadataAlloc>
void* SlabManagerImpl<MetadataAlloc>::PageStartFromId(PageId page_id) const {
  void* slab_start = static_cast<uint8_t*>(heap_start_) +
                     (static_cast<ptrdiff_t>(page_id.Idx()) * kPageSize);
  CK_ASSERT(slab_start >= heap_->Start() && slab_start < heap_->End());
  return slab_start;
}

template <MetadataAllocInterface MetadataAlloc>
PageId SlabManagerImpl<MetadataAlloc>::PageIdFromPtr(const void* ptr) const {
  CK_ASSERT(heap_start_ == heap_->Start());
  CK_ASSERT(ptr >= heap_->Start() && ptr < heap_->End());
  ptrdiff_t diff =
      static_cast<const uint8_t*>(ptr) - static_cast<uint8_t*>(heap_start_);
  return PageId(static_cast<uint32_t>(diff / kPageSize));
}

template <MetadataAllocInterface MetadataAlloc>
std::optional<std::pair<PageId, Slab*>> SlabManagerImpl<MetadataAlloc>::Alloc(
    uint32_t n_pages, SlabType slab_type) {
  auto result = DoAllocWithoutSbrk(n_pages);
  PageId start_id = PageId::Zero();
  Slab* slab;
  if (result.has_value()) {
    start_id = result.value().first;
    slab = result.value().second;
    // No need to allocate entries in the slab map, this will have been done
    // already when this memory was first allocated with `sbrk`.
  } else {
    size_t requested_size = static_cast<size_t>(n_pages) * kPageSize;
    void* slab_start = heap_->sbrk(requested_size);
    if (slab_start == nullptr) {
      return std::nullopt;
    }

    start_id = PageIdFromPtr(slab_start);
    slab = MetadataAlloc::SlabAlloc();
    if (slab == nullptr) {
      return std::nullopt;
    }

    auto result = slab_map_->AllocatePath(start_id, start_id + n_pages - 1);
    if (!result.ok()) {
      // TODO return option in slab_map_->AllocatePath()?
      std::cerr << "Failed to allocate slab map: " << result << std::endl;
      return std::nullopt;
    }
  }

  CK_ASSERT(slab_type != SlabType::kUnmapped && slab_type != SlabType::kFree);
  switch (slab_type) {
    case SlabType::kSmall: {
      slab->InitSmallSlab(start_id, n_pages);
      break;
    }
    case SlabType::kLarge: {
      slab->InitLargeSlab(start_id, n_pages);
      break;
    }
    case SlabType::kMetadata: {
      slab->InitMetadataSlab(start_id, n_pages);
      break;
    }
    case SlabType::kUnmapped:
    case SlabType::kFree: {
      CK_UNREACHABLE();
    }
  }

  // Allocated slabs must map every page to their metadata.
  slab_map_->InsertRange(start_id, start_id + n_pages - 1, slab);
  return std::make_pair(start_id, slab);
}

template <MetadataAllocInterface MetadataAlloc>
void SlabManagerImpl<MetadataAlloc>::Free(Slab* slab) {
  CK_ASSERT(slab->Type() != SlabType::kFree &&
            slab->Type() != SlabType::kUnmapped);
  uint32_t n_pages = slab->Pages();
  if (n_pages == 0) {
    return;
  }

  PageId start_id = slab->StartId();
  if (start_id != PageId::Zero()) {
    Slab* prev_slab = slab_map_->FindSlab(start_id - 1);
    if (prev_slab != nullptr && prev_slab->Type() == SlabType::kFree) {
      start_id = prev_slab->StartId();
      n_pages += prev_slab->Pages();
      RemoveFreeSlab(prev_slab);
      MetadataAlloc::SlabFree(prev_slab);
    }
  }

  PageId end_id = slab->EndId();
  {
    Slab* next_slab = slab_map_->FindSlab(end_id + 1);
    if (next_slab != nullptr && next_slab->Type() == SlabType::kFree) {
      end_id = next_slab->EndId();
      n_pages += next_slab->Pages();
      RemoveFreeSlab(next_slab);
      MetadataAlloc::SlabFree(next_slab);
    }
  }

  slab->InitFreeSlab(start_id, n_pages);
  // We only need to map this slab to the first and last page of the slab, since
  // those will be the only pages queried from this method, and no
  // user-allocated data lives within a free slab.
  slab_map_->Insert(start_id, slab);
  slab_map_->Insert(end_id, slab);

  void* slab_start = PageStartFromId(start_id);
  if (n_pages == 1) {
    auto* slab = new (slab_start) FreeSinglePageSlab();
    single_page_freelist_.InsertFront(slab);
  } else {
    auto* slab = new (slab_start) FreeMultiPageSlab(n_pages);
    multi_page_free_slabs_.Insert(slab);
    // Rb biases toward inserting to the right, so if slab == smallest already,
    // it will certainly not be inserted before it in the tree.
    if (smallest_multi_page_ == nullptr || *slab < *smallest_multi_page_) {
      smallest_multi_page_ = slab;
    }
    CK_ASSERT(multi_page_free_slabs_.Prev(smallest_multi_page_) == nullptr);
  }
}

template <MetadataAllocInterface MetadataAlloc>
std::optional<std::pair<PageId, Slab*>>
SlabManagerImpl<MetadataAlloc>::DoAllocWithoutSbrk(uint32_t n_pages) {
  if (n_pages == 1 && !single_page_freelist_.Empty()) {
    FreeSinglePageSlab* slab_start = single_page_freelist_.Front();
    RemoveSinglePageFreeSlab(slab_start);

    PageId page_id = PageIdFromPtr(slab_start);
    Slab* slab = slab_map_->FindSlab(page_id);
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
  CK_ASSERT(slab_start != nullptr);

  RemoveMultiPageFreeSlab(slab_start);

  PageId page_id = PageIdFromPtr(slab_start);
  uint32_t actual_pages = slab_start->Pages();
  CK_ASSERT(actual_pages >= n_pages);
  if (actual_pages != n_pages) {
    // TODO: free can be simpler, check next to see if is free, if so merge
    // with it. Otherwise do this but no need to coalesce.
    Slab* remainder = MetadataAlloc::SlabAlloc();
    // TODO: This does not work
    remainder->InitFreeSlab(page_id + n_pages, actual_pages - n_pages);
    Free(remainder);
  }

  Slab* slab = slab_map_->FindSlab(page_id);
  return std::make_pair(page_id, slab);
}

template <MetadataAllocInterface MetadataAlloc>
void SlabManagerImpl<MetadataAlloc>::RemoveSinglePageFreeSlab(
    FreeSinglePageSlab* slab_start) {
  single_page_freelist_.Remove(slab_start);
}

template <MetadataAllocInterface MetadataAlloc>
void SlabManagerImpl<MetadataAlloc>::RemoveMultiPageFreeSlab(
    FreeMultiPageSlab* slab_start) {
  if (slab_start == smallest_multi_page_) {
    smallest_multi_page_ = static_cast<FreeMultiPageSlab*>(
        multi_page_free_slabs_.Next(slab_start));
  }
  multi_page_free_slabs_.Remove(slab_start);
  CK_ASSERT(smallest_multi_page_ == nullptr ||
            multi_page_free_slabs_.Prev(smallest_multi_page_) == nullptr);
}

template <MetadataAllocInterface MetadataAlloc>
void SlabManagerImpl<MetadataAlloc>::RemoveFreeSlab(Slab* slab) {
  void* region_start = PageStartFromId(slab->StartId());
  if (slab->Pages() == 1) {
    RemoveSinglePageFreeSlab(
        reinterpret_cast<FreeSinglePageSlab*>(region_start));
  } else {
    RemoveMultiPageFreeSlab(reinterpret_cast<FreeMultiPageSlab*>(region_start));
  }
}

using SlabManager = SlabManagerImpl<GlobalMetadataAlloc>;

}  // namespace ckmalloc
