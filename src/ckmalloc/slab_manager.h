#pragma once

#include <cstdint>
#include <optional>

#include "src/ckmalloc/free_slab.h"
#include "src/ckmalloc/slab_id.h"
#include "src/heap_interface.h"

namespace ckmalloc {

using SlabRbTree = RbTree<FreeMultiPageSlab>;

class SlabManager {
 public:
  explicit SlabManager(bench::Heap* heap);

  // Returns a pointer to the start of a slab with given `SlabId`.
  void* SlabFromId(SlabId slab_id) const;

  // Returns the `SlabId` for the slab containing `ptr`.
  SlabId SlabIdFromPtr(void* ptr) const;

  // Allocates `n_pages` contiguous pages, returning a pointer to the start of
  // the first page in the contiguous block of pages if there is availability,
  // otherwise returning `nullptr`.
  void* AllocRaw(uint32_t n_pages);

  // Allocates `n_pages` contiguous pages, returning the SlabId of the first
  // page in the contiguous block of pages if there is availability, otherwise
  // returning `nullopt`.
  std::optional<SlabId> Alloc(uint32_t n_pages);

  // Frees the slab starting at slab `slab_id` which is `n_pages` long.
  void Free(SlabId slab_id, uint32_t n_pages);

 private:
  // The heap that this SlabManager allocates slabs from.
  bench::Heap* heap_;
  // Cache the heap start from `heap_`, which is guaranteed to never change.
  void* const heap_start_;

  // Single-page slabs are kept in a singly-linked freelist.
  FreeSinglePageSlab* single_page_freelist_ = nullptr;

  // Multi-page slabs are kept in a red-black tree sorted by size.
  SlabRbTree multi_page_free_slabs_;
  // Cache a pointer to the smallest multi-page slab in the tree.
  FreeMultiPageSlab* smallest_multi_page_ = nullptr;
};

}  // namespace ckmalloc
