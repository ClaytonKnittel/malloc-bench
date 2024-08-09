#pragma once

#include <cstdint>
#include <optional>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/free_slab.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
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
  void* SlabStartFromId(PageId page_id) const;

  // Returns the `PageId` for the slab containing `ptr`.
  PageId PageIdFromPtr(const void* ptr) const;

  // Allocates `n_pages` contiguous pages, returning the `PageId` of the first
  // page in the slab, and an allocated uninitialized `Slab` metadata for this
  // range of pages, if there was availability, otherwise returning `nullopt`.
  //
  // Upon returning, it is the responsibility of the caller to initialize the
  // returned `Slab`.
  std::optional<std::pair<PageId, Slab*>> Alloc(uint32_t n_pages);

  // Frees the slab and takes ownership of the `Slab` metadata object.
  void Free(Slab* slab);

 private:
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
void* SlabManagerImpl<MetadataAlloc>::SlabStartFromId(PageId page_id) const {
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
    uint32_t n_pages) {
  if (n_pages == 1 && !single_page_freelist_.Empty()) {
    FreeSinglePageSlab* slab_start = single_page_freelist_.PopFront();

    PageId page_id = PageIdFromPtr(slab_start);
    Slab* slab = slab_map_->FindSlab(page_id);
    return std::make_pair(page_id, slab);
  }
  if (smallest_multi_page_ != nullptr) {
    FreeMultiPageSlab* slab_start;
    if (n_pages <= 2) {
      slab_start = smallest_multi_page_;
    } else {
      slab_start = multi_page_free_slabs_.LowerBound(
          [n_pages](const FreeMultiPageSlab& slab) {
            return slab.Pages() >= n_pages;
          });
    }

    if (slab_start != nullptr) {
      if (slab_start == smallest_multi_page_) {
        smallest_multi_page_ =
            static_cast<FreeMultiPageSlab*>(slab_start->Next());
      }
      multi_page_free_slabs_.Remove(slab_start);
      CK_ASSERT(smallest_multi_page_->Prev() == nullptr);

      PageId page_id = PageIdFromPtr(slab_start);
      uint32_t actual_pages = slab_start->Pages();
      CK_ASSERT(actual_pages >= n_pages);
      if (actual_pages != n_pages) {
        // TODO: free can be simpler, check next to see if is free, if so merge
        // with it. Otherwise do this but no need to coalesce.
        Slab* remainder = MetadataAlloc::SlabAlloc();
        remainder->InitFreeSlab(page_id + n_pages, actual_pages - n_pages);
        Free(remainder);
      }

      Slab* slab = slab_map_->FindSlab(page_id);
      return std::make_pair(page_id, slab);
    }
  }

  size_t requested_size = static_cast<size_t>(n_pages) * kPageSize;
  void* slab_start = heap_->sbrk(requested_size);
  if (slab_start == nullptr) {
    return std::nullopt;
  }

  Slab* slab = MetadataAlloc::SlabAlloc();
  if (slab == nullptr) {
    return std::nullopt;
  }

  return std::make_pair(PageIdFromPtr(slab_start), slab);
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
  if (start_id > PageId::Zero()) {
    Slab* prev_slab = slab_map_->FindSlab(start_id - 1);
    if (prev_slab != nullptr && prev_slab->Type() == SlabType::kFree) {
      start_id = prev_slab->StartId();
      n_pages += prev_slab->Pages();
      MetadataAlloc::SlabFree(prev_slab);
    }
  }

  PageId end_id = slab->EndId();
  {
    Slab* next_slab = slab_map_->FindSlab(end_id + 1);
    if (next_slab != nullptr && next_slab->Type() == SlabType::kFree) {
      end_id = next_slab->EndId();
      n_pages += next_slab->Pages();
      MetadataAlloc::SlabFree(next_slab);
    }
  }

  slab->InitFreeSlab(start_id, n_pages);
  // We only need to map this slab to the first and last page of the slab, since
  // those will be the only pages queried from this method, and no
  // user-allocated data lives within a free slab.
  slab_map_->Insert(start_id, slab);
  slab_map_->Insert(end_id, slab);

  void* slab_start = SlabStartFromId(start_id);
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
    CK_ASSERT(smallest_multi_page_->Prev() == nullptr);
  }
}

using SlabManager = SlabManagerImpl<GlobalMetadataAlloc>;

}  // namespace ckmalloc
