#pragma once

#include <cstddef>

#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/slab_manager.h"

namespace ckmalloc {

class MetadataManager {
 public:
  explicit MetadataManager(SlabId first_slab, SlabManager* slab_manager)
      : last_(first_slab), slab_manager_(slab_manager) {}

  // Allocates `size` bytes aligned to `alignment` and returns a pointer to the
  // beginning of that region.
  //
  // If out of memory, `nullptr` is returned.
  void* Alloc(size_t size,
              std::align_val_t alignment = static_cast<std::align_val_t>(1));

  // Allocate a new slab metadata and return a pointer to it uninitialized.
  Slab* NewSlabMeta();

 private:
  // The most recently allocated metadata slab.
  SlabId last_;
  void* slab_start_;
  // The offset in bytes that the next piece of metadata should be allocated
  // from `last_`.
  uint32_t alloc_offset_ = 0;

  // The slab manager which is used to allocate more metadata slabs if
  // necessary.
  SlabManager* slab_manager_;
};

}  // namespace ckmalloc
