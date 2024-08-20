#pragma once

#include <cstddef>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
class MetadataManagerImpl {
  friend class MetadataManagerFixture;

 public:
  // alloc_offset is by default kPageSize. Initializing this to kPageSize tricks
  // the manager into thinking the last allocated slab is full, even though we
  // have not allocated any slabs on initialization yet. If some metadata has
  // already been allocated, this number can be changed to reflect the number of
  // already-allocated bytes from the first page.
  explicit MetadataManagerImpl(SlabMap* slab_map, SlabManager* slab_manager,
                               PageId last_page,
                               uint32_t alloc_offset = kPageSize)
      : last_(last_page),
        alloc_offset_(alloc_offset),
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
  void FreeSlabMeta(MappedSlab* slab);

 private:
  // The most recently allocated metadata slab.
  PageId last_;

  // The offset in bytes that the next piece of metadata should be allocated
  // from `last_`.
  uint32_t alloc_offset_;

  SlabMap* slab_map_;

  // The slab manager which is used to allocate more metadata slabs if
  // necessary.
  SlabManager* slab_manager_;

  // The head of a singly-linked list of free slabs.
  UnmappedSlab* last_free_slab_ = nullptr;
};

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void* MetadataManagerImpl<MetadataAlloc, SlabMap, SlabManager>::Alloc(
    size_t size, size_t alignment) {
  // Alignment must be a power of two.
  CK_ASSERT_EQ((alignment & (alignment - 1)), 0);
  CK_ASSERT_LE(alignment, kPageSize);
  // Size must already be aligned to `alignment`.
  CK_ASSERT_EQ((size & (alignment - 1)), 0);

  auto current_end = static_cast<uintptr_t>(alloc_offset_);
  uintptr_t aligned_end = AlignUp(current_end, alignment);

  if (aligned_end + size > kPageSize) {
    uint32_t n_pages = (size + kPageSize - 1) / kPageSize;
    std::optional<std::pair<PageId, Slab*>> result =
        slab_manager_->Alloc(n_pages);
    if (!result.has_value()) {
      return nullptr;
    }
    auto [page_id, slab] = std::move(result.value());

    // Clear the slab map entry for the page the slab we just allocated lies in.
    // Since we are calling the non-templated `Alloc` in slab manager, the slab
    // map is not updated for us, and there may be a stale mapping to the
    // previous metadata for this slab.
    slab_map_->Clear(page_id);

    // If we got a slab metadata object back, return it to the freelist since we
    // don't annotate metadata slabs with metadata.
    if (slab != nullptr) {
      MetadataAlloc::SlabFree(static_cast<MappedSlab*>(slab));
    }

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
  CK_ASSERT_LE(alloc_offset_, kPageSize);
  return alloc_start;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
Slab* MetadataManagerImpl<MetadataAlloc, SlabMap, SlabManager>::NewSlabMeta() {
  if (last_free_slab_ != nullptr) {
    Slab* slab = last_free_slab_;
    last_free_slab_ = last_free_slab_->NextUnmappedSlab();
    return slab;
  }

  return reinterpret_cast<Slab*>(
      MetadataAlloc::Alloc(sizeof(Slab), alignof(Slab)));
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void MetadataManagerImpl<MetadataAlloc, SlabMap, SlabManager>::FreeSlabMeta(
    MappedSlab* slab) {
  slab->Init<UnmappedSlab>(last_free_slab_);
  last_free_slab_ = static_cast<Slab*>(slab)->ToUnmapped();
}

using MetadataManager =
    MetadataManagerImpl<GlobalMetadataAlloc, SlabMap, SlabManager>;

}  // namespace ckmalloc
