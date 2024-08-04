#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "src/ckmalloc/allocator.h"
#include "src/ckmalloc/slab_id.h"

namespace ckmalloc {

class SlabManager {
 public:
  // Initializes a new `SlabManager` with an allocator that has not been
  // allocated from yet. The `SlabManager` takes ownership of the allocator.
  //
  // This method will initially allocate a single `MetadataSlab` at the
  // beginning of the allocator region, and place a `SlabManager` at the
  // beginning of that region, returning a pointer to it.
  static SlabManager* InitializeWithEmptyAlloc(Allocator* alloc);

  // Allocates `n_pages` contiguous pages, returning the SlabId of the first
  // page in the contiguous block of pages if there is availability, otherwise
  // returning `nullopt`.
  std::optional<SlabId> Alloc(uint32_t n_pages);

  // The size of slabs in bytes. Typically page size.
  static const size_t kSlabSize;

 private:
  explicit SlabManager(Allocator* alloc);

  // The heap that this SlabManager allocates slabs from.
  Allocator* alloc_;
};

}  // namespace ckmalloc
