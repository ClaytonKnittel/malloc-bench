#pragma once

#include <cstddef>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"

namespace ckmalloc {

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
class FreelistImpl {
 public:
  FreelistImpl(SlabMap* slab_map, SlabManager* slab_manager)
      : slab_map_(slab_map), slab_manager_(slab_manager) {}

  // Allocates a region of memory `size` bytes long, returning a pointer to the
  // beginning of the region.
  void* Alloc(size_t size);

  // Re-allocates a region of memory to be `size` bytes long, returning a
  // pointer to the beginning of the new region and copying the data from `ptr`
  // over. If the returned pointer may equal the `ptr` argument. If `size` is
  // larger than the previous size of the region starting at `ptr`, the
  // remaining data after the size of the previous region is uninitialized, and
  // if `size` is smaller, the data is truncated.
  void* Realloc(void* ptr, size_t size);

  // Frees a region of memory returned from `Alloc`, allowing that memory to be
  // reused by future `Alloc`s.
  void Free(void* ptr);

 private:
  SlabMap* slab_map_;

  SlabManager* slab_manager_;
};

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void* FreelistImpl<MetadataAlloc, SlabMap, SlabManager>::Alloc(size_t size) {
  return nullptr;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void* FreelistImpl<MetadataAlloc, SlabMap, SlabManager>::Realloc(void* ptr,
                                                                 size_t size) {
  return nullptr;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void FreelistImpl<MetadataAlloc, SlabMap, SlabManager>::Free(void* ptr) {}

using Freelist = FreelistImpl<GlobalMetadataAlloc, SlabMap, SlabManager>;

}  // namespace ckmalloc
