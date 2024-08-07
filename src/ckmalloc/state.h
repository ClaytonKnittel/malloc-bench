#pragma once

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

// Allocate slab metadata and return a pointer which may be used by the caller.
// Returns nullptr if out of memory.
Slab* SlabMetadataAlloc();

// Frees slab metadata for later use.
void SlabMetadataFree(Slab* slab);

// Allocates raw memory from the metadata allocator which cannot be freed. This
// is only intended for metadata allocation, never user data allocation.
void* MetadataAlloc(size_t size, size_t alignment);

class State {
 public:
  // Initializes a new `State` with a heap has not been allocated from yet. The
  // `State` takes ownership of the heap.
  static State* InitializeWithEmptyAlloc(bench::Heap* heap);

  // Returns the singleton `State` instance.
  static State* Instance();

  SlabManager* SlabManager() {
    return &slab_manager_;
  }

  MetadataManager* MetadataManager() {
    return &metadata_manager_;
  }

  SlabMap* SlabMap() {
    return &slab_map_;
  }

 private:
  explicit State(bench::Heap* heap);

  // This is the global state instance that is initialized with
  // `InitializeWithEmptyAlloc`.
  static State* state_;

  ckmalloc::SlabMap slab_map_;
  ckmalloc::SlabManager slab_manager_;
  ckmalloc::MetadataManager metadata_manager_;
};

}  // namespace ckmalloc
