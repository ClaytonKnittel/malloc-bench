#include "src/ckmalloc/metadata_manager.h"

#include "src/ckmalloc/slab.h"

namespace ckmalloc {

void* MetadataManager::Alloc(size_t size, std::align_val_t alignment) {
  // Alignment must be a power of two.
  CK_ASSERT((static_cast<size_t>(alignment) &
             (static_cast<size_t>(alignment) - 1)) == 0);
  // Size must already be aligned to `alignment`.
  CK_ASSERT((size & (static_cast<size_t>(alignment) - 1)) == 0);

  auto current_end = static_cast<uintptr_t>(alloc_offset_);
  auto align_mask = static_cast<uintptr_t>(alignment) - 1;
  uintptr_t alignment_increment = (~current_end + 1) & align_mask;

  void* alloc_start =
      static_cast<uint8_t*>(slab_start_) + alloc_offset_ + alignment_increment;
  if (alloc_offset_ > kPageSize) {
    // We do not have enough remaining memory in the heap to allocate this size.
    // TODO: allocate another slab.
    return nullptr;
  }

  alloc_offset_ += alignment_increment + size;
  return alloc_start;
}

}  // namespace ckmalloc
