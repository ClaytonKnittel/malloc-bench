#pragma once

#include <cstddef>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
class MetadataManagerImpl {
  friend class HeapPrinter;
  friend class MetadataManagerFixture;
  friend class GlobalState;
  friend class TraceReplayer;

 public:
  // alloc_offset is by default kPageSize. Initializing this to kPageSize tricks
  // the manager into thinking the last allocated slab is full, even though we
  // have not allocated any slabs on initialization yet. If some metadata has
  // already been allocated, this number can be changed to reflect the number of
  // already-allocated bytes from the first page.
  explicit MetadataManagerImpl(bench::Heap* heap, SlabMap* slab_map)
      : heap_(heap), slab_map_(slab_map), heap_end_(heap->End()) {}

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
  bench::Heap* MetadataHeap();
  const bench::Heap* MetadataHeap() const;

  bench::Heap* const heap_;

  SlabMap* const slab_map_;

  // The end of the already-allocated region of metadata. Metadata heaps are
  // alloc-only, except for slab metadata.
  void* heap_end_;

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

  uintptr_t current_end = reinterpret_cast<uintptr_t>(heap_end_);
  size_t alignment_offset = (~current_end + 1) & (alignment - 1);
  void* alloc_start = PtrAdd(heap_end_, alignment_offset);
  void* alloc_end = PtrAdd(alloc_start, size);

  size_t total_size = alignment_offset + size;
  void* cur_end = MetadataHeap()->sbrk(total_size);
  if (cur_end == nullptr) {
    return nullptr;
  }
  CK_ASSERT_EQ(cur_end, heap_end_);

  heap_end_ = alloc_end;
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
void MetadataManagerImpl<MetadataAlloc, SlabMap>::FreeSlabMeta(
    MappedSlab* slab) {
  slab->Init<UnmappedSlab>(last_free_slab_);
  last_free_slab_ = static_cast<Slab*>(slab)->ToUnmapped();
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
bench::Heap* MetadataManagerImpl<MetadataAlloc, SlabMap>::MetadataHeap() {
  return heap_;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
const bench::Heap* MetadataManagerImpl<MetadataAlloc, SlabMap>::MetadataHeap()
    const {
  return heap_;
}

using MetadataManager = MetadataManagerImpl<GlobalMetadataAlloc, SlabMap>;

}  // namespace ckmalloc
