#include "src/ckmalloc/metadata_manager.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

void* MetadataManager::Alloc(size_t size, size_t alignment) {
  // Alignment must be a power of two.
  CK_ASSERT((alignment & (alignment - 1)) == 0);
  // Size must already be aligned to `alignment`.
  CK_ASSERT((size & (alignment - 1)) == 0);

  auto current_end = static_cast<uintptr_t>(alloc_offset_);
  auto align_mask = static_cast<uintptr_t>(alignment) - 1;
  uintptr_t aligned_end = (current_end + align_mask) & ~align_mask;

  if (size > kPageSize || size > kPageSize - aligned_end) {
    uint32_t n_pages = (size + kPageSize - 1) / kPageSize;
    auto result = slab_manager_->Alloc(n_pages);
    if (!result.has_value()) {
      return nullptr;
    }
    auto [page_id, slab] = std::move(result.value());
    slab->InitMetadataSlab(page_id, n_pages);
    slab_map_->InsertRange(page_id, page_id + n_pages - 1, slab);

    // Decide whether to switch to allocating from this new slab, or stick with
    // the old one. We choose the one with more remaining space.
    size_t remaining_space = n_pages * kPageSize - size;
    if (remaining_space > kPageSize - alloc_offset_) {
      // TODO: shard up the rest of the space in the heap we throw away and give
      // it to the slab freelist?
      last_ = page_id + n_pages - 1;
      alloc_offset_ = kPageSize - remaining_space;
    }

    return slab_manager_->SlabStartFromId(page_id);
  }

  void* alloc_start = static_cast<uint8_t*>(slab_start_) + aligned_end;
  alloc_offset_ = aligned_end + size;
  CK_ASSERT(alloc_offset_ <= kPageSize);
  return alloc_start;
}

}  // namespace ckmalloc
