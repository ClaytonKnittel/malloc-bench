#include "src/ckmalloc/slab_manager.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/free_slab.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

SlabManager::SlabManager(bench::Heap* heap, SlabMap* slab_map)
    : heap_(heap), heap_start_(heap->Start()), slab_map_(slab_map) {}

void* SlabManager::SlabFromId(SlabId slab_id) const {
  void* slab_start = static_cast<uint8_t*>(heap_start_) +
                     (static_cast<ptrdiff_t>(slab_id.Idx()) * kPageSize);
  CK_ASSERT(slab_start >= heap_->Start() && slab_start < heap_->End());
  return slab_start;
}

SlabId SlabManager::SlabIdFromPtr(void* ptr) const {
  CK_ASSERT(heap_start_ == heap_->Start());
  CK_ASSERT(ptr >= heap_->Start() && ptr < heap_->End());
  ptrdiff_t diff =
      static_cast<uint8_t*>(ptr) - static_cast<uint8_t*>(heap_start_);
  return SlabId(static_cast<uint32_t>(diff / kPageSize));
}

std::optional<std::pair<SlabId, Slab*>> SlabManager::Alloc(uint32_t n_pages) {
  if (n_pages == 1 && single_page_freelist_ != nullptr) {
    FreeSinglePageSlab* slab_start = single_page_freelist_;
    single_page_freelist_ = slab_start->NextFree();

    SlabId slab_id = SlabIdFromPtr(slab_start);
    Slab* slab = slab_map_->FindSlab(slab_id);
    return std::make_pair(slab_id, slab);
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

      SlabId slab_id = SlabIdFromPtr(slab_start);
      uint32_t actual_pages = slab_start->Pages();
      CK_ASSERT(actual_pages >= n_pages);
      if (actual_pages != n_pages) {
        // TODO: free can be simpler, check next to see if is free, if so merge
        // with it. Otherwise do this but no need to coalesce.
        Slab* remainder = SlabMetadataAlloc();
        remainder->InitFreeSlab(slab_id + n_pages, actual_pages - n_pages);
        Free(remainder);
      }

      Slab* slab = slab_map_->FindSlab(slab_id);
      return std::make_pair(slab_id, slab);
    }
  }

  size_t requested_size = static_cast<size_t>(n_pages) * kPageSize;
  void* slab_start = heap_->sbrk(requested_size);
  if (slab_start == nullptr) {
    return std::nullopt;
  }

  Slab* slab = SlabMetadataAlloc();
  if (slab == nullptr) {
    return std::nullopt;
  }

  return std::make_pair(SlabIdFromPtr(slab_start), slab);
}

void SlabManager::Free(Slab* slab) {
  CK_ASSERT(slab->Type() != SlabType::kFree &&
            slab->Type() != SlabType::kUnmapped);
  uint32_t n_pages = slab->Pages();
  if (n_pages == 0) {
    return;
  }

  void* slab_start = SlabFromId(slab->StartId());
  // TODO: coalesce.
  if (n_pages == 1) {
    auto* slab = new (slab_start) FreeSinglePageSlab();
    slab->SetNextFree(single_page_freelist_);
    single_page_freelist_ = slab;
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

}  // namespace ckmalloc
