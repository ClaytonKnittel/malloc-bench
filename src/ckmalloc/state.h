#pragma once

#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class State {
 public:
  // Initializes a new `State` with a heap has not been allocated from yet. The
  // `State` takes ownership of the heap.
  static State* InitializeWithEmptyHeap(bench::Heap* heap);

  // Returns the singleton `State` instance.
  static State* Instance();

  SlabMap* SlabMap() {
    return &slab_map_;
  }

  SlabManager* SlabManager() {
    return &slab_manager_;
  }

  MetadataManager* MetadataManager() {
    return &metadata_manager_;
  }

  Freelist* Freelist() {
    return &freelist_;
  }

 private:
  explicit State(bench::Heap* heap);

  // This is the global state instance that is initialized with
  // `InitializeWithEmptyHeap`.
  static State* state_;

  ckmalloc::SlabMap slab_map_;
  ckmalloc::SlabManager slab_manager_;
  ckmalloc::MetadataManager metadata_manager_;
  ckmalloc::Freelist freelist_;
};

}  // namespace ckmalloc
