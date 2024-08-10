#pragma once

#include <cstddef>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

template <MetadataAllocInterface MetadataAlloc>
class MetadataManagerImpl {
  friend class MetadataManagerTest;

 public:
  explicit MetadataManagerImpl(SlabMapImpl<MetadataAlloc>* slab_map,
                               SlabManagerImpl<MetadataAlloc>* slab_manager)
      : last_(PageId::Zero()),
        slab_map_(slab_map),
        slab_manager_(slab_manager) {}

  // Allocates `size` bytes aligned to `alignment` and returns a pointer to the
  // beginning of that region. This memory cannot be released back to the
  // metadata manager.
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

  // The offset in bytes that the next piece of metadata should be allocated
  // from `last_`.
  //
  // Initializing this to kPageSize tricks the manager into
  // thinking the last allocated slab is full, even though we have not allocated
  // any slabs on initialization yet.
  uint32_t alloc_offset_ = kPageSize;

  SlabMapImpl<MetadataAlloc>* slab_map_;

  // The slab manager which is used to allocate more metadata slabs if
  // necessary.
  SlabManagerImpl<MetadataAlloc>* slab_manager_;

  // The head of a singly-linked list of free slabs.
  Slab* last_free_slab_ = nullptr;
};

template <MetadataAllocInterface MetadataAlloc>
void* MetadataManagerImpl<MetadataAlloc>::Alloc(size_t size, size_t alignment) {
  // Alignment must be a power of two.
  CK_ASSERT((alignment & (alignment - 1)) == 0);
  CK_ASSERT(alignment <= kPageSize);
  // Size must already be aligned to `alignment`.
  CK_ASSERT((size & (alignment - 1)) == 0);

  auto current_end = static_cast<uintptr_t>(alloc_offset_);
  auto align_mask = static_cast<uintptr_t>(alignment) - 1;
  uintptr_t aligned_end = (current_end + align_mask) & ~align_mask;

  if (aligned_end + size > kPageSize) {
    uint32_t n_pages = (size + kPageSize - 1) / kPageSize;
    auto result = slab_manager_->Alloc(n_pages, SlabType::kMetadata);
    if (!result.has_value()) {
      return nullptr;
    }
    auto [page_id, slab] = std::move(result.value());

    // Decide whether to switch to allocating from this new slab, or stick with
    // the old one. We choose the one with more remaining space.
    size_t remaining_space = n_pages * kPageSize - size;
    if (remaining_space > kPageSize - alloc_offset_) {
      // TODO: shard up the rest of the space in the heap we throw away and give
      // it to the slab freelist?
      last_ = page_id + n_pages - 1;
      alloc_offset_ = kPageSize - remaining_space;
    }

    return slab_manager_->PageStartFromId(page_id);
  }

  void* slab_start = slab_manager_->PageStartFromId(last_);
  void* alloc_start = static_cast<uint8_t*>(slab_start) + aligned_end;
  alloc_offset_ = aligned_end + size;
  CK_ASSERT(alloc_offset_ <= kPageSize);
  return alloc_start;
}

template <MetadataAllocInterface MetadataAlloc>
Slab* MetadataManagerImpl<MetadataAlloc>::NewSlabMeta() {
  if (last_free_slab_ != nullptr) {
    Slab* slab = last_free_slab_;
    last_free_slab_ = last_free_slab_->NextUnmappedSlab();
    return slab;
  }

  return reinterpret_cast<Slab*>(Alloc(sizeof(Slab), alignof(Slab)));
}

template <MetadataAllocInterface MetadataAlloc>
void MetadataManagerImpl<MetadataAlloc>::FreeSlabMeta(Slab* slab) {
  slab->InitUnmappedSlab(last_free_slab_);
  last_free_slab_ = slab;
}

using MetadataManager = MetadataManagerImpl<GlobalMetadataAlloc>;

}  // namespace ckmalloc
