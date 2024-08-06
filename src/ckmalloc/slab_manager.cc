#include "src/ckmalloc/slab_manager.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "src/ckmalloc/free_slab.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

SlabManager::SlabManager(bench::Heap* heap)
    : heap_(heap), heap_start_(heap->Start()) {}

void* SlabManager::SlabFromId(SlabId slab_id) const {
  void* slab_start = static_cast<uint8_t*>(heap_start_) +
                     (static_cast<ptrdiff_t>(slab_id.Idx()) * kSlabSize);
  CK_ASSERT(slab_start >= heap_->Start() && slab_start < heap_->End());
  return slab_start;
}

SlabId SlabManager::SlabIdFromPtr(void* ptr) const {
  CK_ASSERT(heap_start_ == heap_->Start());
  CK_ASSERT(ptr >= heap_->Start() && ptr < heap_->End());
  ptrdiff_t diff =
      static_cast<uint8_t*>(ptr) - static_cast<uint8_t*>(heap_start_);
  return SlabId(static_cast<uint32_t>(diff / kSlabSize));
}

std::optional<SlabId> SlabManager::Alloc(uint32_t n_pages) {
  if (n_pages == 1 && single_page_freelist_ != nullptr) {
    FreeSinglePageSlab* slab = single_page_freelist_;
    single_page_freelist_ = slab->NextFree();
    return SlabIdFromPtr(single_page_freelist_);
  }
  if (smallest_multi_page_ != nullptr) {
    FreeMultiPageSlab* slab;
    if (n_pages <= 2) {
      slab = smallest_multi_page_;
    } else {
      slab = multi_page_free_slabs_.LowerBound(
          [n_pages](const FreeMultiPageSlab& slab) {
            return slab.Pages() >= n_pages;
          });
    }

    if (slab != nullptr) {
      if (slab == smallest_multi_page_) {
        smallest_multi_page_ = static_cast<FreeMultiPageSlab*>(slab->Next());
      }
      multi_page_free_slabs_.Remove(slab);
      CK_ASSERT(smallest_multi_page_->Prev() == nullptr);

      CK_ASSERT(slab->Pages() >= n_pages);
      SlabId slab_id = SlabIdFromPtr(slab);
      uint32_t actual_pages = slab->Pages();
      Free(slab_id + n_pages, actual_pages - n_pages);
    }
  }

  size_t requested_size = static_cast<size_t>(n_pages) * kSlabSize;
  void* slab_start = heap_->sbrk(requested_size);
  return slab_start != nullptr ? std::optional(SlabIdFromPtr(slab_start))
                               : std::nullopt;
}

void SlabManager::Free(SlabId slab_id, uint32_t n_pages) {
  if (n_pages == 0) {
    return;
  }

  void* slab_start = SlabFromId(slab_id);
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
