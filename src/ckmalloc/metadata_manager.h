#pragma once

#include <cstddef>

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"

namespace ckmalloc {

class MetadataManager {
 public:
  explicit MetadataManager(PageId first_slab, SlabMap* slab_map,
                           SlabManager* slab_manager)
      : last_(first_slab), slab_map_(slab_map), slab_manager_(slab_manager) {}

  // Allocates `size` bytes aligned to `alignment` and returns a pointer to the
  // beginning of that region.
  //
  // If out of memory, `nullptr` is returned.
  void* Alloc(size_t size, size_t alignment = 1);

  // Allocate a new slab metadata and return a pointer to it uninitialized.
  Slab* NewSlabMeta();

  // Frees a slab metadata. This freed slab can be returned from
  // `NewSlabMeta()`.
  void FreeSlabMeta(Slab* slab);

 private:
  // The most recently allocated metadata slab.
  PageId last_;
  void* slab_start_;
  // The offset in bytes that the next piece of metadata should be allocated
  // from `last_`.
  uint32_t alloc_offset_ = 0;

  SlabMap* slab_map_;

  // The slab manager which is used to allocate more metadata slabs if
  // necessary.
  SlabManager* slab_manager_;
};

}  // namespace ckmalloc
