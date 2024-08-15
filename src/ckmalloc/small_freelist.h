#pragma once

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slice.h"

namespace ckmalloc {

class SmallFreelist {
 public:
  explicit SmallFreelist(SlabManager* slab_manager)
      : slab_manager_(slab_manager) {}

  // Allocates a single slice from this small blocks slab, which must not be
  // full.
  // TODO: return multiple once we have a cache?
  AllocatedSlice* TakeSlice(SmallSlab* slab);

  // Returns a slice to the small slab, allowing it to be reallocated.
  void ReturnSlice(SmallSlab* slab, AllocatedSlice* slice);

 private:
  SlabManager* const slab_manager_;
};

}  // namespace ckmalloc
