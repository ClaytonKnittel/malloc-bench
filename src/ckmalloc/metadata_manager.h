#pragma once

#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_id.h"

namespace ckmalloc {

class MetadataManager {
 public:
  explicit MetadataManager(SlabId first_slab)
      : last_(first_slab), alloc_offset_(0) {}

  Slab* NewSlabMeta();

 private:
  // The most recently allocated metadata slab.
  SlabId last_;
  // The offset in bytes that the next piece of metadata should be allocated
  // from `last_`.
  uint32_t alloc_offset_;
};

}  // namespace ckmalloc
