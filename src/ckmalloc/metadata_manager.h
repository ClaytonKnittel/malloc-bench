#pragma once

#include <cstddef>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace ckmalloc {

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
class MetadataManagerImpl {
  friend class HeapPrinter;
  friend class MetadataManagerFixture;

 public:
  // alloc_offset is by default kPageSize. Initializing this to kPageSize tricks
  // the manager into thinking the last allocated slab is full, even though we
  // have not allocated any slabs on initialization yet. If some metadata has
  // already been allocated, this number can be changed to reflect the number of
  // already-allocated bytes from the first page.
  explicit MetadataManagerImpl(bench::HeapFactory* heap_factory,
                               SlabMap* slab_map, size_t heap_idx)
      : heap_factory_(heap_factory), heap_idx_(heap_idx), slab_map_(slab_map) {}

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
  bench::Heap* MetadataHeap();
  const bench::Heap* MetadataHeap() const;

  // The most recently allocated metadata slab.
  void* page_start_ = nullptr;

  // The offset in bytes that the next piece of metadata should be allocated
  // from `last_`.
  uint32_t alloc_offset_ = kPageSize;

  bench::HeapFactory* const heap_factory_;

  // The index of the metadata heap in the heap factory.
  const size_t heap_idx_;

  SlabMap* const slab_map_;

  // The head of a singly-linked list of free slabs.
  UnmappedSlab* last_free_slab_ = nullptr;
};

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void* MetadataManagerImpl<MetadataAlloc, SlabMap>::Alloc(size_t size,
                                                         size_t alignment) {
  // Alignment must be a power of two.
  CK_ASSERT_EQ((alignment & (alignment - 1)), 0);
  CK_ASSERT_LE(alignment, kPageSize);
  // Size must already be aligned to `alignment`.
  CK_ASSERT_EQ((size & (alignment - 1)), 0);

  auto current_end = static_cast<uintptr_t>(alloc_offset_);
  uintptr_t aligned_end = AlignUp(current_end, alignment);

  if (aligned_end + size > kPageSize) {
    uint32_t n_pages = (size + kPageSize - 1) / kPageSize;

    void* alloc = MetadataHeap()->sbrk(n_pages * kPageSize);
    if (alloc == nullptr) {
      return nullptr;
    }

    // Decide whether to switch to allocating from this new slab, or stick with
    // the old one. We choose the one with more remaining space.
    size_t remaining_space = n_pages * kPageSize - size;
    if (remaining_space > kPageSize - alloc_offset_) {
      // TODO: shard up the rest of the space in the heap we throw away and give
      // it to the slab freelist?
      page_start_ = PtrAdd<void>(alloc, (n_pages - 1) * kPageSize);
      alloc_offset_ = kPageSize - remaining_space;
    }

    return alloc;
  }

  void* alloc_start = static_cast<uint8_t*>(page_start_) + aligned_end;
  alloc_offset_ = aligned_end + size;
  CK_ASSERT_LE(alloc_offset_, kPageSize);
  return alloc_start;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
Slab* MetadataManagerImpl<MetadataAlloc, SlabMap>::NewSlabMeta() {
  if (last_free_slab_ != nullptr) {
    Slab* slab = last_free_slab_;
    last_free_slab_ = last_free_slab_->NextUnmappedSlab();
    return slab;
  }

  return reinterpret_cast<Slab*>(
      MetadataAlloc::Alloc(sizeof(Slab), alignof(Slab)));
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
void MetadataManagerImpl<MetadataAlloc, SlabMap>::FreeSlabMeta(Slab* slab) {
  slab->Init<UnmappedSlab>(last_free_slab_);
  last_free_slab_ = slab->ToUnmapped();
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
bench::Heap* MetadataManagerImpl<MetadataAlloc, SlabMap>::MetadataHeap() {
  return heap_factory_->Instance(heap_idx_);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
const bench::Heap* MetadataManagerImpl<MetadataAlloc, SlabMap>::MetadataHeap()
    const {
  return heap_factory_->Instance(heap_idx_);
}

using MetadataManager = MetadataManagerImpl<GlobalMetadataAlloc, SlabMap>;

}  // namespace ckmalloc
