#pragma once

#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/heap_interface.h"

namespace ckmalloc {

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

 private:
  explicit State(bench::Heap* heap);

  // This is the global state instance that is initialized with
  // `InitializeWithEmptyAlloc`.
  static State* state_;

  class SlabManager slab_manager_;
  class MetadataManager metadata_manager_;
};

}  // namespace ckmalloc
