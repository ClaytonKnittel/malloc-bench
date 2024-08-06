#pragma once

#include <cstdint>
#include <optional>

#include "src/ckmalloc/free_slab.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

using SlabRbTree = RbTree<FreeMultiPageSlab>;

class SlabManager {
 public:
  explicit SlabManager(bench::Heap* heap, SlabMap* slab_map);

  // Returns a pointer to the start of a slab with given `SlabId`.
  void* SlabFromId(SlabId slab_id) const;

  // Returns the `SlabId` for the slab containing `ptr`.
  SlabId SlabIdFromPtr(void* ptr) const;

  // Allocates `n_pages` contiguous pages, returning the `SlabId` of the first
  // page in the slab, and an allocated uninitialized `Slab` metadata for this
  // range of pages, if there was availability, otherwise returning `nullopt`.
  //
  // Upon returning, it is the responsibility of the caller to initialize the
  // returned `Slab`.
  std::optional<std::pair<SlabId, Slab*>> Alloc(uint32_t n_pages);

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
  const SlabMap* slab_map_;

  // Single-page slabs are kept in a singly-linked freelist.
  FreeSinglePageSlab* single_page_freelist_ = nullptr;

  // Multi-page slabs are kept in a red-black tree sorted by size.
  SlabRbTree multi_page_free_slabs_;
  // Cache a pointer to the smallest multi-page slab in the tree.
  FreeMultiPageSlab* smallest_multi_page_ = nullptr;
};

}  // namespace ckmalloc
